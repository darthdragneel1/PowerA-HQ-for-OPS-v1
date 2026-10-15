# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.4.x   | :white_check_mark: |
| < 1.4   | :x:                |

## Reporting a Vulnerability

The PowerA PC HQ for macOS team takes the security of our application and user systems seriously. Because this software interacts with low-level USB drivers, IOKit interfaces, and system services (`launchd`), secure handling of input data and local IPC endpoints is paramount.

If you discover a security vulnerability:

1. **Do not create a public GitHub issue.**
2. Please submit vulnerability details using GitHub's **Private Vulnerability Reporting** feature under the **Security** tab of the repository.
3. Include detailed steps to reproduce the issue, proof-of-concept payloads or code, and your environment specifications (macOS version, Mac hardware architecture, and controller model).

### Security Considerations

- **Local IPC Server**: The driver daemon listens exclusively on loopback `127.0.0.1:8765`. It should never bind to external or public network interfaces.
- **IOKit & Permissions**: The driver operates entirely in user-space without requiring kernel extensions (`kexts`) or root privileges.
- **IPC Input Validation**: All HTTP payloads (rumble commands, RGB packets, profile writes) undergo bounds checking and byte sanitization prior to hardware dispatch.
