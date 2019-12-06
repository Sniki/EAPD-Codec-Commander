### Changelog

Oct 3, 2018 v2.7.1

- Detect AppleALC for automatic disable of "Perform Reset" and "Perform Reset on External Wake" (so SSDT-AppleALC.aml not needed usually)

- Added support for AppleALC by reading alc-layout-id property (if present) for layout-id determination

- Added kernel flags -ccoff and -ccpioff


May 1, 2017 v2.6.3

- Added ALC236 configuration (same as ALC283)


Jul 21, 2016 v2.6.2

- Added ALC292 configuration (thanks Joshua Seltzer)


May 28, 2016 v2.6.1

- Added CodecCommanderProbeInit functionality, which allows custom pinconfig, normally done with AppleHDAHardwareConfigDriver patches, to be accomplished with CodecCommander and custom SSDT.  With this feature, AppleHDA patching can be accomplished only with .zml.zlib files in AppleHDA.kext/Contents/Resources and Clover patches.  No need to provide a patched AppleHDAHardwareConfigDriver (nor IOKitPersonality injector).

- There are a few useful scripts to help convert PinConfig data: gen_ahhcd.sh, convert.sh, and extract_hda.sh

- Some optimization regarding IODelay calls while sending codec verbs.

- Fixed some multithread syncronization issues.

- Custom configuration via RMCF (SSDT), can now be merged prior to specific profile selection by setting Version=0x020600 in your RMCF result.

- By setting "LayoutID" in the custom command, Custom Commands can be specific to a layout-id.

- Some code cleanup (avoiding OSCollectionIterator allocations with array iteration, removing unnecessary code from Release version)

- Note: v2.6.0 was a WIP v2.6.1 and was not released


Mar 11, 2016 v2.5.2

-  Bug fix: Custom Commands tagged with On Wake=true, but On Init=false were still being sent.


Feb 06, 2016 v2.5.1

- Fixing a problem (issue #10) with ACPI parsing with arrays of translated entries (such as arrays of dictionaries)


Nov 22, 2015 v2.5.0

- ACPI custom configuration


May 22, 2015 v2.4.0

- CodecCommander PowerHook to monitor codec power state transitions opposed to doing it from Infinite Loop

- Change of providers, again.. using IOHDACodecFunction

- Codec profile lookup rewritten, extended wit subvendor id matching, profile merging with Default corrected

- Not attaching to HDMI codecs by default 

- getUpdateNodes used to return an integer due to typo .. corrected

- Not starting the timer when it's not needed

- Perform Reset on External Wake option added 

- EAPD node updates disabled by default for Gigabyte and Intel desktop boards and ALC892 codec


April 26, 2015 v2.3.4

- Compatibility with Mavericks, needs IOAudioFamily 1.1

- Couple of specifics for ALC269/283 profiles


April 11, 2015 v2.3.2

- Code re-factoring and logging cleanups

- Shell scripts for dumping info for codec nodes

- Fix for Sleep Nodes functionality


April 5, 2015 v2.2.2

- Codec Commander Client introduced (hda-verb analog for OSX) 

- Complete code refactoring by @the-darkvoid

- Support for memory mapped IO and TCSEL update for IntelHDA

- Change of providers to IOAudioDevice and implements IOKit matching

- Using CodecProfile instead of Platform Profile specific to EOMs

- Perform Reset and Sleep Nodes toggles added

September 22, 2014 v2.2.1

- Added customizable delay before getting node count from codec, adjust it if kext fails to start, especially on slower hardware (Intel Core 2 or Atoms)

- Added the ability to cancel updating EAPD status on nodes, because even though node may report EAPD capability it doesn't mean EAPD is actually there - common case for most of desktop codecs that would report 0x14 and 0x1b as being EAPD capable..


September 4, 2014 v2.2.0

- Removed virtual keyboard class that was used for popping audio at wake

- Got rid of audio stream simulation which was using mute/resume NX system events at wake

- Eliminated headphone jack plug/unplug simulation due to being useless

- New algo to prevent jack sense loss and unpredicted codec power state transitions by performing the following at system wake (see extract from HDA spec in performCodecReset() for details):

  1. resetting codec function group (NID=0x01)

  2. forcefully setting codec power state to D3 Hot

- Added Gigabyte OEM name support

- CodecCommander can now be used on desktop boards with problematic codecs (some ALC892 implementations), where jack sense  and audio loss is a problem after sleep, even though there are no EAPDs to enable. Codec will be reset and set to D3 power state, thus simulating a scenario as if the codec was started cold

- Not monitoring EAPD state when audio stream is active anymore, since resetting the codec causes it to be enabled properly after first PIO.

- MCP workaround removed due to being useless with new reset functionality. MCP chipset can also use the setting to prevent audio loss after fugue sleep, which was previously limited to 4 PIOs 

- Node configuration no longer needed as EAPD enabled nodes are determined automatically when Codec Commander starts

- Fixed a bug with memory allocation which would cause system to lock up and restart when unloading the kext manually


July 13, 2014 v2.1.2:

- Add a procedure to set PinCaps on headphone node to Hpone enable, then Hphone disable to trigger a jack insertion event. Helps with audio loss on Intel controllers

- Add extra node to update in case codec utilizes more than 2x EAPDs on output nodes

- Add an ability to specify HDEF device address (location), Default Intel location is @1B

- Platform Profiles are brought back to support multiple machines with a single kext. Default config serves as a base, OEM config overrides default config. See examples…

- CodecCommander will first try to read OEM data reported to IODeviceTree by Clover/bareBoot, if none are reported it will try to get OEM info from IOService on PS2K device (RM,oem-id\RM,oem-table-id) and only then from DSDT header.

- Detect HDEF Controller chipset based on vendor-id data reported from IORegistry

- Properly power down EAPDs across sleep on Intel chipset

- Add support for nVidia MCP79/MCP7A HDA controllers that do not report EAPD state when reading response from IRR. A workaround is applied if such a chipset is detected to limit PIO count to 4, which is enough to keep speaker and headphone EAPDs enabled in 10.9.2+

- Remove unnecessary probing in order to prevent unresolved symbols when compiling with earlier SDKs for newer system version.


Feb 06, 2014 v2.1.1:

- Now monitoring codec power state from HDADriver, EAPD will re-enable after fugue-sleep state now too!

- Add a workaround to jack sense loss after 2 PIO operations (use Apple - Sleep and tap any key after 5 seconds, jack sense should work thereafter).

- Got rid of Platform Profiles as this function has little to no use

- Implement the ability to specify the EngineOutput number (Engine Output Number in config)

- EAPD status checking loop can go infinitely

- Get EAPD status bit if multiple updating is not requested, but bezel popping is 


Feb 01, 2014 v2.1.0:

- Implement virtual keyboard, add an ability to invoke mute-unmute event at wake to produce an audio stream at wake (Generate Stream toggle in config)

- Parse IORegistry to determine if audio stream is up on EngineOutput, if stream is up we are going to check whether EAPD bit is set

- Ability to get response from IRR register in order read EAPD status bit

- Added option to update EAPD status bit multiple times upon wake (Update Multiple Times in config), timeout will be called after 2 PIO operations

- Almost a complete rewrite, antipop no longer required (unless user decides otherwise)


Oct 15, 2013 v1.0.1:

- Performs PIO operation at cold boot as well as wake that will set EAPD status bit

- Required antipop 1.0.2 to generate audio stream upon wake or there will be no jack sense at wake

- Initial release
