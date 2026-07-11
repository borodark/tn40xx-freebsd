# tn40xx — Tehuti Networks 10GbE Driver for FreeBSD 15

Kernel driver for Tehuti Networks TN40xx-based 10 Gigabit Ethernet adapters on FreeBSD 15.0-RELEASE.

Tehuti Networks ceased operations in January 2020. Their 10GbE adapters were sold under multiple brands and remain widely available on the used market. FreeBSD has never had in-tree support for these devices. This driver fills that gap.

## Supported Hardware

| Adapter | Device ID | PHY | Connector |
|---------|-----------|-----|-----------|
| **Tehuti TN9210** | 0x4024 | Marvell MV88X3120 | 10GBase-T (RJ45) |
| Tehuti TN9030 | 0x4020 | CX4 | CX4 |
| Tehuti TN9310 | 0x4022 | QT2025 | SFP+ |
| Tehuti TN9610 | 0x4026 | TLK10232 | SFP+ |
| Tehuti TN9510 | 0x4025 | Aquantia AQR105 | 10GBase-T |
| D-Link DXE-810S | 0x4022 | QT2025 | SFP+ |
| D-Link DXE-810T | 0x4025 | AQR105 | 10GBase-T |
| ASUS XG-C100F | 0x4022 | QT2025 | SFP+ |

**Bold** = tested. Others should work but are untested.

The TN9710P (MV88X3310) and TN9710Q (MV88E2010) are **not supported** — their PHY source files are not available.

## Quick Start

### 1. Extract PHY firmware (MV88X3120 adapters only)

The Marvell MV88X3120 PHY requires firmware that cannot be freely redistributed. A script extracts it from the AKiTiO macOS driver:

```sh
cd tools
./extract_firmware.sh
```

This downloads the AKiTiO driver package, extracts the firmware blob from the macOS kext binary, and generates `sys/dev/tn40xx/MV88X3120_phy.h`. SFP+ and CX4 adapters do not need this step.

### 2. Build

```sh
cd sys/modules/tn40xx
make
```

If building out-of-tree (not from `/usr/src`):

```sh
make SRCTOP=/path/to/tn40xx-freebsd
```

### 3. Load

```sh
kldload ./tn40xx.ko
```

Or copy to `/boot/modules/` for persistent use:

```sh
cp tn40xx.ko /boot/modules/
echo 'tn40xx_load="YES"' >> /boot/loader.conf
```

### 4. Configure

```sh
ifconfig tn400 inet 192.168.1.1/24 up
```

## Thunderbolt

This driver works with adapters connected via Thunderbolt enclosures (AKiTiO Thunder3, Sonnet Echo, etc.) on Mac hardware running FreeBSD. There are two caveats:

### Adapters must be connected before boot

FreeBSD does not support Thunderbolt hot-plug. The adapter must be physically connected when the machine powers on, so the Thunderbolt controller enumerates the PCIe device during POST.

### MSI interrupts do not work through Thunderbolt

Intel Thunderbolt controllers (Falcon Ridge, Cactus Ridge) use an Internal Connection Manager (ICM) firmware that creates PCIe tunnels for config and memory-mapped I/O. However, the ICM does not configure the Thunderbolt switch fabric to forward MSI memory writes (address range 0xFEE00000) upstream to the CPU. MSI writes are silently dropped.

This driver includes a workaround: a dedicated kernel thread polls the device every 50 microseconds for RX/TX completion and link status changes. This provides functional networking without interrupts.

#### Throughput (Thunderbolt, polled mode)

Tested between two MacBook Pros connected directly with a Cat6 cable, each with a TN9210 behind Thunderbolt:

| Direction | Throughput | Retransmits |
|-----------|-----------|-------------|
| Single stream TX | 3.7 Gbps | 0 |
| Single stream RX | 5.1 Gbps | 0 |
| Bidirectional | 4.2 Gbps (aggregate) | 0 |

With native MSI (non-Thunderbolt PCIe slot), the hardware should achieve close to 10 Gbps line rate.

#### MSI root cause details

The investigation ruled out all standard PCIe causes:

- PCI bridge memory windows do not claim the MSI address range
- Bus mastering is enabled on all bridges in the chain
- No ACS (Access Control Services) blocking
- No AER (Advanced Error Reporting) errors on the data path
- MSI address, vector, and target CPU are all correctly programmed

The Thunderbolt switch fabric silently drops the MSI writes. This is a firmware limitation of the ICM on Falcon Ridge and Cactus Ridge controllers. Fixing it would require implementing a software Thunderbolt connection manager to configure tunnel hop registers directly — effectively porting Linux's `drivers/thunderbolt/` subsystem to FreeBSD.

FreeBSD's `pci_alloc_msi()` also programs an incorrect APIC destination ID for Thunderbolt-attached devices (targets wrong CPU), which is a separate kernel bug.

## Driver Architecture

```
tn40.c                    FreeBSD attach/detach, ISR, poll thread, ifnet glue
tn40.h                    Driver private structures
tn40_hw.c                 Hardware abstraction: DMA, ring management, link state
tn40_hw.h                 Register definitions, PHY dispatch
tn40_mbuf.c               mbuf DMA mapping
tn40_fw.h                 Device firmware data

MV88X3120_phy.c           Marvell MV88X3120 PHY: firmware upload, link, speed
MV88X3120_phy_FreeBSD.c   FreeBSD ifmedia integration for MV88X3120
MV88X3120_phy.h           PHY firmware blob (generated, not in repo)

AQR105_phy.c              Aquantia AQR105 PHY driver
QT2025_phy.c              QT2025 SFP+ PHY driver
TLK10232_phy.c            TI TLK10232 SFP+ PHY driver
CX4.c                     CX4 PHY driver
*_FreeBSD.c               FreeBSD ifmedia wrappers for each PHY
```

## History

This driver is based on [scfcode/tn40xx](https://github.com/scfcode/tn40xx), a community salvage of the original Tehuti Networks FreeBSD driver targeting FreeBSD 11/12. The MV88X3120 PHY code was ported from [acooks/tn40xx-driver](https://github.com/acooks/tn40xx-driver) (Linux).

Changes from the scfcode base:

- Ported to FreeBSD 15 opaque `if_t` network interface API
- Added MV88X3120 PHY support (10GBase-T copper)
- Added polling thread for Thunderbolt MSI workaround
- Added INTx fallback when MSI allocation fails
- Fixed multicast for FreeBSD 13+ (`if_foreach_lladdr`)
- Fixed `DRIVER_MODULE` for FreeBSD 14+ (removed devclass_t)
- Replaced direct `ifp->if_input()` with `if_input()`

## License

BSD 2-Clause. See [LICENSE](LICENSE).
