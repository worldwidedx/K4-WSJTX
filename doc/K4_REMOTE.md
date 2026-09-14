# K4 Remote architecture and operation

On Windows, K4 WSJT-X loads only the Xiph Opus DLL installed beside
`wsjtx.exe`. This is intentional: Windows on ARM includes a different
`System32\\opus.dll` that is not a safe substitute for the QK4 codec runtime.

K4 WSJT-X preserves the upstream WSJT-X decoder, waterfall, message sequencing,
logging, reporting, and user workflow. It replaces the radio and local
sound-card boundary with the Elecraft K4 Remote protocol learned and exercised
in QK4 Mobile.

The upstream baseline for this fork is commit
`ccdfaf3c1c109010d15399674ce278167cfde848` from the official
`https://github.com/WSJTX/wsjtx` repository.

## Connection settings

The Radio settings page contains the only available radio interface:

- Host name, IPv4 address, or `.local` name.
- TCP port. The QK4 defaults are 9205 for plain TCP and 9204 for TLS.
- K4 remote password.
- TLS 1.2-or-later PSK mode and optional PSK identity. The password is the
  pre-shared key. Certificate verification is intentionally not used in a PSK
  session.
- Audio encoding EM0 (raw 32-bit), EM1 (raw 16-bit), EM2 (Opus integer), or
  EM3 (Opus float).
- Streaming latency SL0 through SL7.

For plain TCP the client sends the lowercase hexadecimal SHA-384 digest of the
password, as QK4 does. For TLS, authentication happens in the TLS-PSK
handshake; no second password message is sent. The saved password uses the same
lightweight obfuscation format as QK4. This avoids casual disclosure in the
settings file but is not encryption of credentials at rest.

After authentication the client sends `RDY;`, `K41;`, `ER1;`, the selected
`EMn;`, and the selected `SLn;`. TCP low-delay and keepalive socket options are
enabled, and timestamped `PING...;` CAT messages are sent once per second. As
in QK4, `.local` names are resolved asynchronously with an IPv4 preference and
the selected SL tier is reasserted after the initial state dump.

## CAT and mode control

K4 CAT is handled directly; Hamlib, serial CAT, OmniRig, TCI, external PTT, VOX,
and local sound cards are not selectable paths in this fork. VFO A/B frequency,
split, mode, PTT, test state, input state, RF-power setting, and transmit
metering are read from the K4 CAT stream.

Digital transmission selects K4 DATA-A with `MD6;DT0;` and uses explicit
`TX;`/`RX;` commands. It never depends on VOX. Setting changes are made only as
part of an explicit operator action or the normal WSJT-X transmit workflow; a
connection by itself requests readback and does not silently alter operator
settings.

The main-window **Pwr** slider controls the K4 RF-power setting with QK4's
`PCnnnr;` encoding. It provides 0.1 W steps from 0.1 through 10.0 W (`L` range)
and whole-watt steps from 11 through 110 W (`H` range), requests `PC;` readback,
and displays the radio-confirmed setting. Transverter `X`-range readback is
shown in mW; the slider is read-only in that range. Legacy WSJT-X audio-level
and per-band attenuation memories are not applied to the radio.

## Network audio

K4 audio frames use the QK4 framing markers and big-endian payload length. The
audio subheader is version 1 with a wrapping sequence number, EM mode, a
little-endian frame-sample count, and the 12 kHz sample-rate code.

Receive audio is decoded to 12 kHz mono from the K4 main receiver (the left
channel) and is written directly into the normal WSJT-X decode buffer. This
uses the existing network-audio integration point, so waterfall and decode
timing remain within the upstream application.

FT8 and FT4 transmit audio use WSJT-X's already generated and filtered waveform,
downsampled from 48 kHz to the K4 stream's native 12 kHz. Packet sizes follow
the selected SL tier: 240, 480, 720, or 1440 samples. EM2 and EM3 require the
standard Xiph Opus shared library (`opus.dll`, `libopus.so`, or `libopus.dylib`)
to be available at runtime. EM0 and EM1 do not require Opus.
Windows packaging picks up either `opus.dll` or the MSYS2 `libopus-0.dll` name;
the DEB and RPM package metadata declares the corresponding Opus runtime.

## Remote input calibration and TX protection

The **Calibrate Remote Input** action follows QK4's safety policy:

1. Read the current K4 TEST state and require positive `TS1` confirmation after
   enabling TEST if necessary.
2. Select DATA-A and start a protected 1500 Hz tone.
3. Enable and poll `TM` metering every 250 ms.
4. Search from 1/32 gain for a stable raw ALC value of 3 through 5.
5. Reduce gain automatically at raw ALC 6 or higher, stop immediately at 10,
   and reject any speech compression or RF output observed during TEST.
6. Stop after 15 seconds if no stable safe value is found, then restore TEST if
   the application enabled it and require `TS0` confirmation.

The saved calibration is bound to the K4 host and port, EM codec, line input,
mic gain, compression, and TX equalizer readback. A changed context requires a
new calibration for optimized drive. Transmission remains available without
one and starts at the conservative 1/32 drive level. Every live transmission
retains fresh-meter and audio-delivery watchdogs plus automatic downward drive
adjustment; calibration does not turn off live protection.

## Operator acceptance test

Start with the K4 connected to a dummy load or with its station output otherwise
made safe. An acceptance test should never rely on software alone to prevent
unintended RF.

1. Install and start K4 WSJT-X. Complete the normal station callsign and grid
   setup, then open **File > Settings > Radio**. **Elecraft K4 Remote** is the
   only radio choice; no local sound-card selection is required.
2. Enter the same host and password used by QK4. For an initial plain-TCP test,
   leave TLS off and use port 9205. Select EM1 and SL3, then click **Test CAT**.
   The button should turn green. Click **OK**; the displayed frequency should
   follow VFO A.
3. Verify receive audio: the waterfall should advance and FT8 signals should
   decode on an active band. Change VFO A on the K4 and confirm that the
   application follows it. Move the **Pwr** slider and confirm the K4 changes to
   the displayed power, then change power at the K4 and confirm the slider
   follows the `PC` readback. Reopen **File > Settings > Radio**.
4. Click **Calibrate Remote Input**. The status must report protected TEST mode,
   converge to a saved input gain, turn PTT off, and restore the original TEST
   state. Treat any reported TEST-state, RF-output, compression, stale-meter, or
   ALC fault as a failed test; do not continue to an on-air test.
5. Close and reopen the application. Confirm that the connection settings and
   saved calibration return, and that receive decoding still works.
6. Repeat the connection test with **TLS 1.2+ with password as PSK** enabled and
   port 9204. Leave the optional identity blank unless the K4/QK4 setup uses a
   specific identity. A green **Test CAT**, live frequency, and advancing
   waterfall confirm the TLS CAT and audio path together.
7. Only after those checks pass, perform one low-power FT8 transmission into a
   dummy load. Confirm DATA-A, zero RF during TEST, normal RF only after TEST is
   off, prompt return to receive, and no protection warning. Changing the host,
   port, audio encoding, or relevant K4 input settings should invalidate the
   optimized calibration and make the next transmission use conservative drive.

For codec coverage, repeat receive-only testing with EM0, EM1, EM2, and EM3.
EM0/EM1 exercise raw audio; EM2/EM3 exercise the bundled Opus runtime. Repeat at
the desired SL tier and use the lowest-latency tier that remains free of audio
dropouts on the real network.

## Developer verification

The focused tests build independently with Qt 6 on a development machine:

```powershell
cmake -S tests/k4remote -B build-k4remote-tests -G Ninja
cmake --build build-k4remote-tests
ctest --test-dir build-k4remote-tests --output-on-failure
```

They cover fragmented and coalesced K4 frames, SHA-384 password formatting,
raw audio channel/encoding behavior, the calibration target, emergency ALC
trip behavior, and compilation of the complete K4 transceiver and factory.
Building the complete desktop application still uses the upstream WSJT-X build
requirements, including Qt 5, Boost, Hamlib, FFTW, and a Fortran compiler.

## Continuous integration and releases

GitHub Actions builds every push and pull request targeting `master` on five
targets: Windows x86_64, Linux x86_64, Linux aarch64, macOS Intel, and macOS
Apple Silicon. Linux jobs produce DEB, RPM, and AppImage packages. macOS jobs
produce PKG installers, and Windows produces an NSIS installer. Artifacts use
the `K4-WSJT-X-<version>-<platform>` name.

The runtime test suite fails a build when Qt cannot load TLS or does not expose
an OpenSSL PSK cipher. Linux AppImages bundle `libopus.so.0` explicitly. The
macOS package follows QK4 by bundling OpenSSL 3 and `libopus.0.dylib` inside the
application's Frameworks directory and preferring those copies at runtime.

Pushing `build/vX.Y.Z` or `build/vX.Y.Z-rcN` runs the same five-platform matrix
and publishes a GitHub Release directly to `worldwidedx/K4-WSJTX`, but only
after every installer-grade artifact exists. The tag's numeric version must
match `CMakeLists.txt`.

Without Apple secrets, macOS CI still creates an ad-hoc-signed test package.
Trusted distribution and notarization require these repository secrets:

- `DEVELOPER_ID_CERTIFICATE_P12`
- `DEVELOPER_ID_CERTIFICATE_PASSWORD`
- `DEVELOPER_ID_INSTALLER_P12`
- `DEVELOPER_ID_INSTALLER_PASSWORD`
- `APPLE_ID`
- `APPLE_APP_SPECIFIC_PASSWORD`
- `APPLE_TEAM_ID`

The release workflow uses only the repository-scoped GitHub token. It contains
no upstream mirror remote, force push, or `CROSS_REPO_TOKEN` path.

The socket, protocol parser, and transport timers are QObject children of the
K4 transceiver. This is required because WSJT-X moves the transceiver to its rig
thread after construction. Keeping any of those objects as unparented value
members leaves them on the GUI thread and makes TLS callbacks and timer starts
cross-thread operations, which can corrupt the heap during disconnects.
