05Oct2026           Notes on WSJT-X Installation for Mac OS X
                    -----------------------------------------

If you have already downloaded a previous version of WSJT-X then I suggest 
you change the name in the Applications folder from WSJT-X to WSJT-X_previous 
before proceeding.  

I recommend that you follow the installation instructions especially if you
are moving from v2.7 to v3.0 or later, of WSJT-X or you have upgraded macOS.

Double-click on the wsjtx-...-Darwin.dmg file you have downloaded from the main web-site.
NB: Make sure that you leave this window open for the remaining installation steps.

WSJT-X uses native POSIX shared memory for its decoder. No System V shared-memory
limit adjustment, sysctl login item, or reboot is required for this version.

If you previously installed com.wsjtx.sysctl.plist, it can remain installed.
Older WSJT-X versions or other applications may still need those settings.

Drag the WSJT-X app to your preferred location, such as Applications, and close the window.

You need to configure your sound card.   Visit Applications > Utilities > Audio MIDI 
Setup and select your sound card and then set Format to be "48000Hz 2ch-16bit" for 
input and output.  On rare occasions problems with audio output to your rig can be
corrected if you select 44100Hz for output format.

Now double-click on the WSJT-X app and two windows will appear.  Select Preferences 
under the WSJT-X Menu and fill in various station details on the General panel.   
I recommend checking the 4 boxes under the Display heading and the first 4 boxes under 
the Behaviour heading.

Depending on your macOS you might see a pop-up window suggesting that wsjtx wants to use the
microphone.   What this means is that audio input must be allowed.  Agree.

Next visit the Audio panel and select the Audio Codec you use to communicate between 
WSJT-X and your rig.   There are so many audio interfaces available that it is not 
possible to give detailed advice on selection.  If you have difficulties contact me.   
Note the location of the Save Directory.  Decoded wave forms are located here.

Look at the Reporting panel.  If you check the "Prompt me" box, a logging panel will appear 
at the end of the QSO.  Visit Section 11 of the User Guide for information about log files
and how to access them.

Finally, visit the Radio panel.  WSJT-X is most effective when operated with CAT 
control.  You will need to install the relevant Mac device driver for your rig, 
and then re-launch WSJT-X. Return to the Radio panel in Preferences and in 
the "Serial port" panel select your driver from the list that is presented. 

You may need a device driver for your Mac. The USB/UART Bridge chip inside the Icom,
Yaesu and Kenwood radios is a Silicon Labs USB to UART Bridge Controller and the Mac
drivers are available here:

https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers

Visit the SiLabs site and download v6 for a Mac. Then in WSJT-X if you use the drop-down menu
for Serial Port you should see something like /dev/tty.SLAB_USBtoUART if the driver has been
installed correctly.  Make sure you read the release notes that come with the driver.

WSJT-X needs the Mac clock to be accurate.  Visit System Preferences > Date & Time 
and make sure that Date and Time are set automatically.  The drop-down menu will 
normally offer you several time servers to choose from.

On the Help menu, have a look at the new Online User's Guide for operational hints 
and tips and possible solutions to any problem you might have.

Please email me if you have problems.

--- John G4KLA     (g4kla@rmnjmn.co.uk)

Addendum:  Running multiple instances of WSJT-X.

See "Section 16.2 Frequently asked Questions" in the User Guide. Each instance
allocates its own decoder shared memory; no System V limit adjustment is required.

If two instances of WSJT-X are running, it is likely that you might need additional
audio devices, from two rigs for example.  Visit Audio MIDI Setup and create an Aggregate Device
which will allow you to specify more than one interface.  I recommend you consult Apple's guide
on combining multiple audio interfaces which is at https://support.apple.com/en-us/HT202000.  

2.  Preventing WSJT-X from being put into 'sleep' mode (App Nap).

In normal circumstances an application which has not been directly accessed for a while can be
subject to App Nap which means it is suspended until such time as its windows are accessed.  If
you find that WSJT-X seems disabled check this by opening Applications > Utilities > Activity Monitor and
then select Energy and look at the column marked App Nap.  If you see wsjtx marked "Yes" then you need
to disable App Nap by opening a Terminal window and typing:
    defaults  write  NSGlobalDomain  NSAppSleepDisabled  -bool  YES
This will disable App Nap for all applications.  If you wish to reverse this type:
    defaults delete  NSGlobalDomain  NSAppSleepDisabled
