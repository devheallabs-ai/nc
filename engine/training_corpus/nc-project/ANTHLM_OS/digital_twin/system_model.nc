// ══════════════════════════════════════════════════════════════════
//  HiveANT — Digital Twin System Model
//
//  Maintains a continuously updated model of the monitored system:
//    - Service topology
//    - Dependency graph
//    - Data flows
//    - Deployment history
//  Agents query this model to understand system structure.
// ══════════════════════════════════════════════════════════════════

to twin_init:
    purpose: "Initialize the digital twin system model"
    shell("mkdir -p memory/twins")

    set twin to {
        "version": "1.0.0",
        "services": {},
        "dependencies": [],
        "data_flows": [],
        "deployments": [],
        "infrastructure": {},
        "service_count": 0,
        "last_updated": time_now(),
        "created_at": time_now()
    }

    write_file("memory/twins/system_model.json", json_encode(twin))
    log "DIGITAL TWIN: System model initialized"
    respond with twin

to twin_register_service with service_name, service_type, endpoints, metadata:
    purpose: "Register a service in the digital twin"
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
    otherwise:
        set twin to {"services": {}, "dependencies": [], "data_flows": [], "deployments": [], "infrastructure": {}, "service_count": 0, "last_updated": time_now(), "created_at": time_now()}

    set twin.services[service_name] to {
        "name": service_name,
        "type": service_type,
        "endpoints": endpoints,
        "metadata": metadata,
        "health": "unknown",
        "version": "",
        "last_deploy": "",
        "registered_at": time_now(),
        "last_checked": time_now()
    }
    set twin.service_count to twin.service_count + 1
    set twin.last_updated to time_now()
    write_file(twin_path, json_encode(twin))

    log "DIGITAL TWIN: Registered service " + service_name
    respond with {"service": service_name, "status": "registered"}

to twin_add_dependency with from_service, to_service, dependency_type, protocol:
    purpose: "Add a dependency relationship between services"
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
        set dep to {
            "from": from_service,
            "to": to_service,
            "type": dependency_type,
            "protocol": protocol,
            "added_at": time_now()
        }
        set twin.dependencies to twin.dependencies + [dep]
        set twin.last_updated to time_now()
        write_file(twin_path, json_encode(twin))
        respond with {"dependency": dep, "total": len(twin.dependencies)}
    otherwise:
        respond with {"error": "Twin not initialized"}

to twin_record_deployment with service_name, version, changes, deployed_by:
    purpose: "Record a deployment in the digital twin"
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
        set deploy to {
            "service": service_name,
            "version": version,
            "changes": changes,
            "deployed_by": deployed_by,
            "deployed_at": time_now()
        }
        set twin.deployments to twin.deployments + [deploy]
        if twin.services[service_name]:
            set twin.services[service_name].version to version
            set twin.services[service_name].last_deploy to time_now()
        set twin.last_updated to time_now()
        write_file(twin_path, json_encode(twin))
        respond with {"deployment": deploy, "total_deployments": len(twin.deployments)}
    otherwise:
        respond with {"error": "Twin not initialized"}

to twin_update_health with service_name, health_status, metrics:
    purpose: "Update a service's health status in the digital twin"
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
        if twin.services[service_name]:
            set twin.services[service_name].health to health_status
            set twin.services[service_name].latest_metrics to metrics
            set twin.services[service_name].last_checked to time_now()
            set twin.last_updated to time_now()
            write_file(twin_path, json_encode(twin))
            respond with {"service": service_name, "health": health_status}
        otherwise:
            respond with {"error": "Service not registered in twin"}
    otherwise:
        respond with {"error": "Twin not initialized"}

to twin_get_topology:
    purpose: "Get the full system topology"
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
        respond with {
            "services": twin.services,
            "dependencies": twin.dependencies,
            "data_flows": twin.data_flows,
            "service_count": twin.service_count,
            "dependency_count": len(twin.dependencies)
        }
    otherwise:
        respond with {"error": "Twin not initialized"}

to twin_get_impact with service_name:
    purpose: "Get impact analysis — what services are affected if this service fails"
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))

        ask AI to "Analyze this system topology to determine the blast radius if '{{service_name}}' fails.

SERVICES: {{twin.services}}
DEPENDENCIES: {{twin.dependencies}}
DATA FLOWS: {{twin.data_flows}}

Return ONLY valid JSON:
{
  \"failed_service\": \"string\",
  \"directly_affected\": [\"string\"],
  \"indirectly_affected\": [\"string\"],
  \"blast_radius_percent\": 0,
  \"critical_path\": true,
  \"cascade_risk\": \"high|medium|low\",
  \"mitigation\": [\"string\"]
}" save as impact

        respond with impact
    otherwise:
        respond with {"error": "Twin not initialized"}

to twin_status:
    purpose: "Get digital twin status"
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
        respond with {
            "service_count": twin.service_count,
            "dependency_count": len(twin.dependencies),
            "deployment_count": len(twin.deployments),
            "last_updated": twin.last_updated
        }
    otherwise:
        respond with {"status": "not_initialized"}
