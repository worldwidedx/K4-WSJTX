---
title: K4 WSJT-X — Remote FT8 and FT4 for the Elecraft K4
description: Stream Elecraft K4 receive and transmit audio and control your radio over the network. Download K4 WSJT-X and follow the remote FT8 and FT4 setup guide.
---

# K4 WSJT-X — Remote FT8 and FT4 for the Elecraft K4

K4 WSJT-X is a desktop app for operating FT8, FT4, and other WSJT-X digital modes with an Elecraft K4 over a network. It streams receive audio from the K4 directly into the waterfall and decoder, sends transmit audio back to the radio, and carries CAT rig control over the same K4 Remote connection. There’s no USB audio cable, virtual sound card, or separate CAT connection to configure.

[Download K4 WSJT-X for Windows, macOS, or Linux](https://github.com/worldwidedx/K4-WSJTX/releases)

Choose the installer for your operating system and CPU from the release’s **Assets**. Check the release notes for platform availability and installation details, especially for prerelease builds.

## Quick setup

### 1. Prepare the Elecraft K4

Connect the K4 to Ethernet and use firmware that supports K4 Remote server operation. On the K4, tap the **globe** icon, open **Server Settings**, set a **Password**, and set **Max Clients** to at least **1**. Note the radio’s IP address or hostname. For the first connection, use a computer on the same local network as the radio. If the K4 is already configured as a remote server, use its existing connection details. [Elecraft K4 Operating Manual](https://ftp.elecraft.com/K4/Manuals%20Downloads/K4%20Built-In%20Operating%20Manual%20rev%20D14/K4BuiltInOperatingManualrevD14.html)

### 2. Connect K4 WSJT-X to the radio

Open **Settings** (**File → Settings** on Windows and Linux). In **General**, enter your callsign and grid locator. In **Radio**, look for **Elecraft K4 Remote** and enter:

| Setting | What to enter |
| --- | --- |
| **Host** | The K4’s IPv4 address or hostname, such as `192.168.1.50`. |
| **Password** | The case-sensitive password from the K4’s **Server Settings**. |
| **Port** and **TLS** | Use **9204** with **TLS 1.2+ with password as PSK** enabled, or **9205** with TLS off for plain TCP on a trusted local network. If your station uses a different port, enter that port. |
| **TLS identity** | Leave blank unless your station uses a specific identity. |
| **Audio encoding** | Start with **EM1 - raw 16-bit**. |
| **Streaming latency** | Start with **3 (SL3)**. |

Click **Test CAT**. When it turns green, click **OK**. The frequency display should follow the K4’s VFO A.

### 3. Verify receive audio

Select **FT8** and an active band, then click **Monitor** if it’s off. Look for a moving waterfall and decoded signals in **Band Activity**. Keep the computer’s clock synchronized for timed digital modes.

### 4. Set up transmit audio

With the station connected to a dummy load, open **Settings → Radio** and run **Calibrate Remote Input**. The app uses K4 **TEST** mode to set transmit audio drive. Wait for successful completion and confirm the original TEST state is restored before trying a low-power transmission into the dummy load. Calibration is optional. Without a saved calibration, the app starts with conservative audio drive.

The **Pwr** slider controls the K4’s RF power. The app selects **DATA-A** for digital transmission and monitors ALC during transmit.

## K4 streaming over your network

For operation away from the station, enter a host or IP address reachable from your computer. Use TLS on a connection that crosses the public internet. If your station uses a VPN, use the K4 address reachable through that VPN.

- **Test CAT stays red.** Check the host, port, TLS setting, remote password, and available client slots on the K4.
- **Test CAT is green but the waterfall is still.** Check that **Monitor** is on and try **EM1** with latency **3** to verify the receive stream.
- **Audio drops out.** Try a higher streaming latency. If bandwidth is limited, try an Opus encoding (**EM2** or **EM3**).

For K4-specific details, see [K4 Remote architecture and operation](https://github.com/worldwidedx/K4-WSJTX/blob/master/doc/K4_REMOTE.md).

## Core WSJT-X features

[Official WSJT-X website](https://wsjtx.github.io/wsjtx/)
