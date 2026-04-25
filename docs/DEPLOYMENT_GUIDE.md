# Deployment & Infrastructure Guide

> Everything you need to deploy, configure DNS, and maintain the DevHeal Labs
> family of websites on GitHub Pages with GoDaddy-managed domains.
>
> Note: this guide mixes `nc-lang` deployment with broader DevHeal Labs website
> operations. Trim or split the non-`nc-lang` sections before publishing a
> standalone public `nc-lang` repository.

---

## Table of Contents

1. [GitHub Pages Setup](#1-github-pages-setup)
2. [Domain Architecture](#2-domain-architecture)
3. [NC UI Deployment (ncui.devheallabs.in)](#3-nc-ui-deployment-ncuidevheallabsin)
4. [Updating Websites](#4-updating-websites)
5. [Cost Summary](#5-cost-summary)
6. [CI/CD with GitHub Actions](#6-cicd-with-github-actions)
7. [Quick Reference Card](#7-quick-reference-card)

---

## 1. GitHub Pages Setup

### 1.1 Prerequisites

- A GitHub account (free tier is fine).
- A GitHub organization (e.g. `DevHealLabs`) or personal account.
- Optionally a second org for the NC language (e.g. `nc-lang`).
- A domain registered on GoDaddy (e.g. `devheallabs.in`).

### 1.2 Repository Structure

You need **6 repositories** on GitHub:

| # | Local folder | GitHub repository | Custom domain |
|---|---|---|---|
| 1 | `devheallabs-website/` | `DevHealLabs/DevHealLabs.github.io` | `devheallabs.in` |
| 2 | `nc-lang-website/` | `nc-lang/nc-lang.github.io` | `nc.devheallabs.in` |
| 3 | `nc-ui/website/` | `DevHealLabs/nc-ui` | `ncui.devheallabs.in` |
| 4 | `hiveant-website/` | `DevHealLabs/hiveant` | `hiveant.devheallabs.in` |
| 5 | `swarmops-website/` | `DevHealLabs/swarmops` | `swarmops.devheallabs.in` |
| 6 | `neuraledge-website/` | `DevHealLabs/neuraledge` | `neuraledge.devheallabs.in` |

> **Naming convention:** The "user/org site" repo must be named
> `<username>.github.io`. Project repos can be any name.

### 1.3 Creating a Repository and Enabling Pages

Repeat these steps for each website:

1. **Create the repo** on GitHub (public, no README).
2. Push the website files (see the automated script below, or do it manually):
   ```bash
   mkdir /tmp/deploy && cd /tmp/deploy
   cp /path/to/website/index.html .
   cp /path/to/website/CNAME .        # if using a custom domain
   git init -b main
   git add -A
   git commit -m "Initial deploy"
   git remote add origin git@github.com:DevHealLabs/reponame.git
   git push -u origin main --force
   ```
3. **Enable GitHub Pages** in the repo:
   - Go to **Settings > Pages**.
   - Source: **Deploy from a branch**.
   - Branch: `main`, folder: `/ (root)`.
   - Click **Save**.
4. **Set the custom domain** (still in Settings > Pages):
   - Enter the domain (e.g. `hiveant.devheallabs.in`).
   - Click **Save**. GitHub will add/update the `CNAME` file in the repo.
5. **Enforce HTTPS**: Check the "Enforce HTTPS" box once DNS is verified.

### 1.4 Automated Deployment Script

A convenience script is provided at `scripts/deploy-websites.sh`:

```bash
# Deploy all 5 websites
./scripts/deploy-websites.sh DevHealLabs nc-lang

# Preview without deploying
./scripts/deploy-websites.sh --dry-run DevHealLabs nc-lang
```

The script will, for each website:
- Create a temporary directory.
- Copy `index.html`, `CNAME`, and any other assets.
- Initialize a git repo, commit, and force-push to the correct GitHub repo.

### 1.5 GoDaddy DNS Configuration

Log in to GoDaddy and navigate to:
**My Products > Domain > DNS > Manage DNS**.

#### 1.5.1 Apex Domain (devheallabs.in)

Add **four A records** pointing to GitHub's servers:

| Type | Name | Value | TTL |
|------|------|-------|-----|
| A | @ | 185.199.108.153 | 600 |
| A | @ | 185.199.109.153 | 600 |
| A | @ | 185.199.110.153 | 600 |
| A | @ | 185.199.111.153 | 600 |

Optionally add a `www` CNAME so `www.devheallabs.in` works:

| Type | Name | Value | TTL |
|------|------|-------|-----|
| CNAME | www | DevHealLabs.github.io | 600 |

#### 1.5.2 Subdomain CNAME Records

Each subdomain points to the **GitHub org/user** `.github.io` address:

| Type | Name | Value | TTL |
|------|------|-------|-----|
| CNAME | nc | nc-lang.github.io | 600 |
| CNAME | ncui | DevHealLabs.github.io | 600 |
| CNAME | hiveant | DevHealLabs.github.io | 600 |
| CNAME | swarmops | DevHealLabs.github.io | 600 |
| CNAME | neuraledge | DevHealLabs.github.io | 600 |

> **Important:** CNAME values must include the trailing period in some DNS
> providers, but GoDaddy usually adds it automatically. Do *not* add `https://`
> or a path -- just the hostname.

#### 1.5.3 CNAME File in Each Repo

Each repo's root must contain a file named `CNAME` (no extension) with a single
line: the custom domain for that site.

```
# devheallabs-website/CNAME
devheallabs.in
```

```
# nc-lang-website/CNAME
nc.devheallabs.in
```

```
# nc-ui/website/CNAME
ncui.devheallabs.in
```

```
# hiveant-website/CNAME
hiveant.devheallabs.in
```

```
# swarmops-website/CNAME
swarmops.devheallabs.in
```

```
# neuraledge-website/CNAME
neuraledge.devheallabs.in
```

### 1.6 HTTPS Enforcement

After DNS propagation, go to each repo's **Settings > Pages** and check
**Enforce HTTPS**. GitHub provisions a free TLS certificate via Let's Encrypt.

If the checkbox is greyed out, DNS has not propagated yet -- wait 15-30 minutes
and try again (can take up to 24-48 hours in rare cases).

### 1.7 Troubleshooting

| Problem | Solution |
|---------|----------|
| "Domain not yet verified" | Wait for DNS propagation (15 min to 48 hours). Verify DNS records with `dig devheallabs.in` or https://dnschecker.org. |
| HTTPS checkbox greyed out | DNS not propagated yet. Wait and retry. |
| 404 on custom domain | Ensure the correct branch is selected in Pages settings and `index.html` exists at root. |
| Mixed content warnings | Make sure all asset URLs in HTML use `https://` or protocol-relative `//`. |
| "CNAME already taken" | Another repo in the same org already has that CNAME. Remove it from the old repo first. |
| Subdomain not loading | Verify the CNAME record in GoDaddy points to `<org>.github.io` (not the repo URL). Verify the `CNAME` file in the repo matches the subdomain exactly. |
| Changes not appearing | GitHub Pages can cache aggressively. Hard-refresh (`Ctrl+Shift+R`) or wait a few minutes. Check the Actions tab for deployment status. |

---

## 2. Domain Architecture

```
devheallabs.in  (GoDaddy)
|
|-- A records --> GitHub IPs (185.199.108-111.153)
|      |
|      '--> DevHealLabs/DevHealLabs.github.io  (main company site)
|
|-- CNAME: nc.devheallabs.in --> nc-lang.github.io
|      |
|      '--> nc-lang/nc-lang.github.io  (NC language site)
|
|-- CNAME: ncui.devheallabs.in --> DevHealLabs.github.io
|      |
|      '--> DevHealLabs/nc-ui  (NC UI playground & docs)
|
|-- CNAME: hiveant.devheallabs.in --> DevHealLabs.github.io
|      |
|      '--> DevHealLabs/hiveant  (HiveANT project site)
|
|-- CNAME: swarmops.devheallabs.in --> DevHealLabs.github.io
|      |
|      '--> DevHealLabs/swarmops  (SwarmOps project site)
|
|-- CNAME: neuraledge.devheallabs.in --> DevHealLabs.github.io
|      |
|      '--> DevHealLabs/neuraledge  (NeuralEdge project site)
```

### How GitHub Pages Routes Custom Domains

- **Org/user site** (`<org>.github.io` repo): served at the apex or `www`.
- **Project sites** (any other repo with Pages enabled): GitHub reads the `CNAME`
  file to decide which repo to serve for a given custom domain. This is why the
  `CNAME` file is critical -- without it GitHub cannot route the subdomain to the
  correct repo.

---

## 3. NC UI Deployment (ncui.devheallabs.in)

The NC UI is the web-based playground and documentation site for the NC language.
Its source lives at `nc-ui/website/` in the monorepo.

### 3.1 Website Files

The `nc-ui/website/` folder contains:

| File | Purpose |
|------|---------|
| `index.html` | Landing page for NC UI |
| `playground.html` | Interactive NC language playground |
| `nc-ui.js` | Client-side JavaScript for the playground |
| `CNAME` | Custom domain (`ncui.devheallabs.in`) |
| `.nojekyll` | Tells GitHub Pages to skip Jekyll processing |
| `404.html` | Custom 404 error page |

> **Why `.nojekyll`?** GitHub Pages runs Jekyll by default, which ignores files
> starting with `_` and can interfere with static assets. The `.nojekyll` file
> disables this behavior so all files are served as-is.

### 3.2 GoDaddy DNS Setup

Add a CNAME record in GoDaddy DNS for the `ncui` subdomain:

| Type | Name | Value | TTL |
|------|------|-------|-----|
| CNAME | ncui | DevHealLabs.github.io | 600 |

### 3.3 GitHub Pages Setup

1. **Create the repo** `DevHealLabs/nc-ui` on GitHub (public).
2. Push the website files:
   ```bash
   mkdir /tmp/deploy && cd /tmp/deploy
   cp ~/Documents/nc-main/nc-ui/website/* .
   cp ~/Documents/nc-main/nc-ui/website/.nojekyll .
   git init -b main
   git add -A
   git commit -m "Deploy NC UI"
   git remote add origin git@github.com:DevHealLabs/nc-ui.git
   git push -u origin main --force
   ```
3. Go to **Settings > Pages** in the `DevHealLabs/nc-ui` repo:
   - Source: **Deploy from a branch**.
   - Branch: `main`, folder: `/ (root)`.
   - Click **Save**.
4. Set the custom domain to `ncui.devheallabs.in` and click **Save**.
5. Once DNS propagates, check **Enforce HTTPS**.

### 3.4 Updating NC UI

After making changes to files in `nc-ui/website/`:

```bash
cd /tmp && rm -rf deploy && mkdir deploy && cd deploy
cp ~/Documents/nc-main/nc-ui/website/* .
cp ~/Documents/nc-main/nc-ui/website/.nojekyll .
git init -b main && git add -A
git commit -m "Update NC UI"
git remote add origin git@github.com:DevHealLabs/nc-ui.git
git push -u origin main --force
```

### 3.5 Verify Deployment

```bash
# Check DNS
dig ncui.devheallabs.in +short

# Verify the site is live
curl -sI https://ncui.devheallabs.in | head -5

# Check HTTPS certificate
curl -vI https://ncui.devheallabs.in 2>&1 | grep -i "subject\|issuer\|expire"
```

---

## 4. Updating Websites

### 4.1 Update an Existing Website

1. Edit the files in the local website folder (e.g. `hiveant-website/index.html`).
2. Re-run the deploy script:
   ```bash
   ./scripts/deploy-websites.sh DevHealLabs nc-lang
   ```
   Or deploy manually:
   ```bash
   cd /tmp && rm -rf deploy && mkdir deploy && cd deploy
   cp ~/Documents/nc-main/hiveant-website/* .
   git init -b main && git add -A
   git commit -m "Update HiveANT site"
   git remote add origin git@github.com:DevHealLabs/hiveant.git
   git push -u origin main --force
   ```
3. Changes appear within 1-5 minutes (GitHub's CDN cache).

### 4.2 Add a New Project Website

1. Create a new folder in the monorepo: `newproject-website/`.
2. Add at minimum an `index.html` and a `CNAME` file containing the subdomain.
3. Create a new repo on GitHub (e.g. `DevHealLabs/newproject`).
4. Add an entry to `scripts/deploy-websites.sh` in the `WEBSITES` array:
   ```bash
   "newproject-website|${GITHUB_ORG}/newproject|NewProject site"
   ```
5. Add a CNAME record in GoDaddy:
   ```
   CNAME  newproject  DevHealLabs.github.io  600
   ```
6. Run the deploy script.
7. Enable Pages and HTTPS in the new repo's settings.

### 4.3 Custom 404 Page

Create a `404.html` file in the root of the deployed repo. GitHub Pages will
automatically serve it for any URL that doesn't match a file.

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Page Not Found</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            display: flex; justify-content: center; align-items: center;
            min-height: 100vh; margin: 0;
            background: #0a0a0a; color: #ffffff;
        }
        .container { text-align: center; }
        h1 { font-size: 4rem; margin-bottom: 0.5rem; }
        p  { font-size: 1.2rem; color: #888; }
        a  { color: #00d4ff; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>404</h1>
        <p>This page doesn't exist.</p>
        <p><a href="/">Go home</a></p>
    </div>
</body>
</html>
```

---

## 5. Cost Summary

| Item | Cost |
|------|------|
| GitHub Pages hosting | Free |
| SSL/TLS certificates (Let's Encrypt via GitHub) | Free |
| Subdomain DNS records | Free (included with domain) |
| GitHub repositories (public) | Free |
| **Domain renewal** (`devheallabs.in` on GoDaddy) | ~$10-15/year |

**Total recurring cost: domain renewal only.**

---

## 6. CI/CD with GitHub Actions

Instead of running the deploy script manually, you can set up a GitHub Actions
workflow in the main NC monorepo that auto-deploys a website whenever its files
change.

### 6.1 Example Workflow

Create `.github/workflows/deploy-hiveant.yml` (repeat/adapt for each website):

```yaml
name: Deploy HiveANT Website

on:
  push:
    branches: [main]
    paths:
      - 'hiveant-website/**'

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout monorepo
        uses: actions/checkout@v4

      - name: Deploy to GitHub Pages
        uses: peaceiris/actions-gh-pages@v4
        with:
          # A personal access token with repo scope, stored as a secret
          personal_token: ${{ secrets.DEPLOY_TOKEN }}
          publish_dir: ./hiveant-website
          external_repository: DevHealLabs/hiveant
          publish_branch: main
          cname: hiveant.devheallabs.in
```

### 6.2 Setup Steps

1. Create a **Personal Access Token** (classic) on GitHub with `repo` scope.
2. Add it as a secret named `DEPLOY_TOKEN` in the NC monorepo's settings:
   **Settings > Secrets and variables > Actions > New repository secret**.
3. Create one workflow file per website (or a single matrix workflow).
4. Push to `main`. The workflow triggers only when files in the relevant folder
   change.

### 6.3 Single Matrix Workflow (Advanced)

To deploy all websites from one workflow file, create
`.github/workflows/deploy-websites.yml`:

```yaml
name: Deploy Websites

on:
  push:
    branches: [main]
    paths:
      - '*-website/**'

jobs:
  detect-changes:
    runs-on: ubuntu-latest
    outputs:
      matrix: ${{ steps.set-matrix.outputs.matrix }}
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 2

      - id: set-matrix
        run: |
          CHANGED=$(git diff --name-only HEAD~1 HEAD | grep -oP '^\K[^/]+-website' | sort -u)
          JSON="["
          for dir in $CHANGED; do
            case "$dir" in
              devheallabs-website) repo="DevHealLabs/DevHealLabs.github.io"; cname="devheallabs.in" ;;
              nc-lang-website)     repo="nc-lang/nc-lang.github.io";        cname="nc.devheallabs.in" ;;
              nc-ui-website)       repo="DevHealLabs/nc-ui";               cname="ncui.devheallabs.in" ;;
              hiveant-website)     repo="DevHealLabs/hiveant";              cname="hiveant.devheallabs.in" ;;
              swarmops-website)    repo="DevHealLabs/swarmops";             cname="swarmops.devheallabs.in" ;;
              neuraledge-website)  repo="DevHealLabs/neuraledge";           cname="neuraledge.devheallabs.in" ;;
              *) continue ;;
            esac
            JSON="$JSON{\"dir\":\"$dir\",\"repo\":\"$repo\",\"cname\":\"$cname\"},"
          done
          JSON="${JSON%,}]"
          echo "matrix=$JSON" >> "$GITHUB_OUTPUT"

  deploy:
    needs: detect-changes
    if: needs.detect-changes.outputs.matrix != '[]'
    runs-on: ubuntu-latest
    strategy:
      matrix:
        site: ${{ fromJson(needs.detect-changes.outputs.matrix) }}
    steps:
      - uses: actions/checkout@v4

      - name: Deploy ${{ matrix.site.dir }}
        uses: peaceiris/actions-gh-pages@v4
        with:
          personal_token: ${{ secrets.DEPLOY_TOKEN }}
          publish_dir: ./${{ matrix.site.dir }}
          external_repository: ${{ matrix.site.repo }}
          publish_branch: main
          cname: ${{ matrix.site.cname }}
```

---

## 7. Quick Reference Card

### 7.1 Domain / Repo / DNS Overview

| Domain | GitHub Repo | DNS Record Type | DNS Name | DNS Value |
|--------|-------------|-----------------|----------|-----------|
| `devheallabs.in` | `DevHealLabs/DevHealLabs.github.io` | A (x4) | @ | 185.199.108-111.153 |
| `www.devheallabs.in` | (same as above) | CNAME | www | DevHealLabs.github.io |
| `nc.devheallabs.in` | `nc-lang/nc-lang.github.io` | CNAME | nc | nc-lang.github.io |
| `ncui.devheallabs.in` | `DevHealLabs/nc-ui` | CNAME | ncui | DevHealLabs.github.io |
| `hiveant.devheallabs.in` | `DevHealLabs/hiveant` | CNAME | hiveant | DevHealLabs.github.io |
| `swarmops.devheallabs.in` | `DevHealLabs/swarmops` | CNAME | swarmops | DevHealLabs.github.io |
| `neuraledge.devheallabs.in` | `DevHealLabs/neuraledge` | CNAME | neuraledge | DevHealLabs.github.io |

### 7.2 Common Commands

```bash
# Deploy all websites
./scripts/deploy-websites.sh DevHealLabs nc-lang

# Dry run (preview only)
./scripts/deploy-websites.sh --dry-run DevHealLabs nc-lang

# Check DNS propagation
dig devheallabs.in +short
dig nc.devheallabs.in +short
dig ncui.devheallabs.in +short
dig hiveant.devheallabs.in +short

# Verify GitHub Pages DNS (should return GitHub IPs)
dig DevHealLabs.github.io +short

# Flush local DNS cache (macOS)
sudo dscacheutil -flushcache; sudo killall -HUP mDNSResponder

# Check HTTPS certificate
curl -vI https://devheallabs.in 2>&1 | grep -i "subject\|issuer\|expire"
```

### 7.3 GoDaddy DNS Settings Location

1. Log in at https://www.godaddy.com.
2. Click **My Products** (top-right menu).
3. Find your domain (`devheallabs.in`) and click **DNS** (or Manage DNS).
4. You will see a table of DNS records. Add/edit A and CNAME records here.
5. Set TTL to **600 seconds** (10 min) for faster propagation during setup.
   Increase to 3600 (1 hour) once everything is stable.

### 7.4 GitHub Pages IPs (for A Records)

```
185.199.108.153
185.199.109.153
185.199.110.153
185.199.111.153
```

These rarely change, but the canonical source is:
https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site

---

*Last updated: 2026-03-22*
