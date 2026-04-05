terraform {
  required_version = ">= 1.5"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }

  backend "s3" {
    bucket         = "nc-terraform-state"
    key            = "nc/terraform.tfstate"
    region         = "us-east-1"
    encrypt        = true
    dynamodb_table = "nc-terraform-locks"
  }
}

provider "aws" {
  region = var.aws_region

  default_tags {
    tags = {
      Project     = "nc"
      Environment = var.environment
      ManagedBy   = "terraform"
    }
  }
}

# ═══════════════════════════════════════════════════════════
#  VPC & Networking
# ═══════════════════════════════════════════════════════════

module "vpc" {
  source  = "terraform-aws-modules/vpc/aws"
  version = "~> 5.0"

  name = "nc-${var.environment}"
  cidr = var.vpc_cidr

  azs             = var.availability_zones
  private_subnets = var.private_subnet_cidrs
  public_subnets  = var.public_subnet_cidrs

  enable_nat_gateway   = true
  single_nat_gateway   = var.environment != "production"
  enable_dns_hostnames = true
  enable_dns_support   = true

  tags = {
    "kubernetes.io/cluster/nc-${var.environment}" = "shared"
  }

  private_subnet_tags = {
    "kubernetes.io/role/internal-elb" = 1
  }

  public_subnet_tags = {
    "kubernetes.io/role/elb" = 1
  }
}

# ═══════════════════════════════════════════════════════════
#  ECS Cluster (Fargate)
# ═══════════════════════════════════════════════════════════

module "ecs" {
  source = "../modules/ecs"

  cluster_name = "nc-${var.environment}"
  vpc_id       = module.vpc.vpc_id
  subnet_ids   = module.vpc.private_subnets

  container_image  = var.container_image
  container_port   = 8080
  cpu              = var.ecs_cpu
  memory           = var.ecs_memory
  desired_count    = var.ecs_desired_count
  min_count        = var.ecs_min_count
  max_count        = var.ecs_max_count

  environment_variables = {
    NC_PORT             = "8080"
    NC_LOG_FORMAT       = "json"
    NC_LOG_LEVEL        = var.log_level
    NC_AUDIT_FORMAT     = "json"
    NC_MAX_WORKERS      = tostring(var.max_workers)
    NC_CORS_ORIGIN      = var.cors_origin
  }

  secrets = {
    NC_AI_KEY     = var.ai_key_secret_arn
    NC_JWT_SECRET = var.jwt_secret_arn
  }

  health_check_path = "/health"
  certificate_arn   = var.certificate_arn

  tags = {
    Service = "nc"
  }
}

# ═══════════════════════════════════════════════════════════
#  Secrets Manager
# ═══════════════════════════════════════════════════════════

resource "aws_secretsmanager_secret" "nc_secrets" {
  name                    = "nc/${var.environment}/secrets"
  recovery_window_in_days = var.environment == "production" ? 30 : 0
}

# ═══════════════════════════════════════════════════════════
#  CloudWatch Log Group
# ═══════════════════════════════════════════════════════════

resource "aws_cloudwatch_log_group" "nc" {
  name              = "/ecs/nc-${var.environment}"
  retention_in_days = var.log_retention_days

  tags = {
    Application = "nc"
  }
}

# ═══════════════════════════════════════════════════════════
#  CloudWatch Alarms
# ═══════════════════════════════════════════════════════════

resource "aws_cloudwatch_metric_alarm" "high_cpu" {
  alarm_name          = "nc-${var.environment}-high-cpu"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 2
  metric_name         = "CPUUtilization"
  namespace           = "AWS/ECS"
  period              = 300
  statistic           = "Average"
  threshold           = 80
  alarm_description   = "NC service CPU utilization is above 80%"

  dimensions = {
    ClusterName = module.ecs.cluster_name
    ServiceName = module.ecs.service_name
  }
}

resource "aws_cloudwatch_metric_alarm" "high_memory" {
  alarm_name          = "nc-${var.environment}-high-memory"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 2
  metric_name         = "MemoryUtilization"
  namespace           = "AWS/ECS"
  period              = 300
  statistic           = "Average"
  threshold           = 80
  alarm_description   = "NC service memory utilization is above 80%"

  dimensions = {
    ClusterName = module.ecs.cluster_name
    ServiceName = module.ecs.service_name
  }
}
