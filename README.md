# Cortex-M Bare-Metal Template (STM32F407)

A minimal starting point for bare-metal ARM Cortex-M4 projects on the
STM32F407. No HAL, no CMSIS, no vendor SDK. Includes a full vector table,
a working linker script, a superset register map, and a build system that
needs no edits when you add files.

Use this as the base for every new project: it eliminates the header-drift
bug where each project accumulates its own partial register map and
inevitably loses track of which symbols exist.

## How to Use

### Option 1: GitHub "Use this template"

After this repo is created on GitHub, click **Settings → Template
repository** to enable the green "Use this template" button. Future
projects then start with a single click and no git history.

### Option 2: Clone and re-init

```bash
git clone https://github.com/YOUR_USER/cortex-m-baremetal-template my-project
cd my-project
rm -rf .git
git init
