# Orders Demo

Small runnable demo showing the NC backend and NC UI frontend as separate files.

## Files

- `service.nc` - backend CRUD service for orders
- `app.ncui` - frontend dashboard source
- `app.html` - built frontend artifact after running the UI build command

## Quick Start

From `nc-lang/engine/orders_demo`:

```powershell
..\build\nc.exe serve service.nc
```

In another terminal from the same folder:

```powershell
..\build\nc.exe ui build app.ncui
```

That produces `app.html` in this folder.

## Purpose

This demo exists to validate that NC can keep backend and UI generation separate while still supporting a simple full-stack workflow.