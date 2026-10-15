---
name: Bug Report
about: Create a report to help us improve PowerA PC HQ for macOS
title: "[BUG] "
labels: ["bug"]
assignees: ""
---

**Describe the Bug**
A clear and concise description of what the bug is.

**Controller Connection Mode**
- [ ] Mode 1: Wired USB-C Cable (`VID: 0x20D6`, `PID: 0x4025`)
- [ ] Mode 2: 2.4 GHz USB Wireless Dongle (`PID: 0x400C / 0x4013 / 0x4027`)
- [ ] Mode 3: Bluetooth

**System Information**
- macOS Version: [e.g. macOS 14.5 Sonoma, macOS 15.0 Sequoia]
- Mac Model & Architecture: [e.g. M1 Pro, M3 Max, Intel Core i7]
- Controller Model: [e.g. PowerA OPS v1 (Model 1531), OPS v3 Pro]
- Firmware Version (if known):

**To Reproduce**
Steps to reproduce the behavior:
1. Connect controller via '...'
2. Open 'PowerA PC HQ'
3. Navigate to '...'
4. See error

**Expected Behavior**
A clear and concise description of what you expected to happen.

**Driver Logs & Output**
Run the following in terminal and paste output here:
```bash
curl -s http://127.0.0.1:8765/api/status
cat /tmp/powera_driver.log
```

**Screenshots**
If applicable, add screenshots to help explain your problem.
