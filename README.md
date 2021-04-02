## Codec Commander
====

[![Build Status](https://github.com/Sniki/EAPD-Codec-Commander/workflows/CI/badge.svg?branch=master)](https://github.com/Sniki/EAPD-Codec-Commander/actions)

### What is the purpose of this?
Used for updating EAPD (External Amplifier) state on HDA (High Definition Audio) codecs that use given amp on Speaker or Headphone nodes (both, or even extra ones in some cases). In OS X EAPD gets powered down across sleep so audio remains non functional after waking the machine up.

Usually, this external amp is present on laptops and ITX board, most common on machines with ALC269, ALC665 and similar codecs. When machine falls asleep the amp is powered down and after waking up, even though it seems like audio is working, there is no sound coming from speaker/headphones because amp requires a codec command verb sent to it in order to powered up.

This kext is intended to take care of this.

Additionally, starting from v2.2.0, Codec Commander can now solve a problem that plagues some desktop boards without EAPDs, but with a problem that causes to loose jack sense and sometimes audio upon wake. One of these boards is H87-HD3 with ALC892 onboard audio codec. For workaround, the codec is reset at wake, much like VoodooHDA acts, in order be treated by AppleHDA the same way as before sleep.

### How is this useful over patched IOAudioFamily?
People used to rely on custom IOAudioFamily - Apple's open source files were altered, incorporating a method (originally coded by km9) to update the EAPD after sleep. What's bad about this kind of approach is that it required sources for modification to happen… and as everyone probably knows by now, Apple tends to delay the release of sources for 3 weeks to 2 month after OS updates get released. 

No more waiting for sources, no need to be searching for a kext that matches your node layout and no need to have different kexts for different OS X versions (generations, if you will). 

### How do I enable it?

You have to edit settings inside Info.plist - see Profiles sections as well. There are multiple Default settings defined which have values of:

				<key>Default</key>
				<dict>
					<key>Check Infinitely</key>
					<false/>
					<key>Check Interval</key>
					<integer>3000</integer>
					<key>Custom Commands</key>
					<array/>
					<key>Perform Reset</key>
					<true/>
					<key>Perform Reset on External Wake</key>
					<true/>
					<key>Perform Reset on EAPD Fail</key>
					<false/>
					<key>Send Delay</key>
					<integer>300</integer>
					<key>Update Nodes</key>
					<true/>
					<key>Sleep Nodes</key>
					<true/>
				</dict>
				
About these in more details:

* Check Infinitely - CC will keep monitoring the codec power state transitions, *as of today this is useless* as CodecCommanderPowerHook attached to AppleHDADriver to detect power state changes on demand.

* Check Interval - the time in ms for above setting to check the codec power state, again, *as of today this is useless*.

* Perform Reset - whether to perform complete codec reset (returns codec in cold-boot state) at wake from sleep if codec behaves weird after sleep.

* Perform Reset on External Wake - same as above, but for fugue-sleep, when you break the machine entering sleep prematurely.

* Perform Reset on EAPD Fail - self explanatory - if EAPD update fails at wake then CC will perform complete codec reset in an attempt to recover the codec.

* Send Delay - the time in ms that CC needs to wait before sending commands to the codec, otherwise it may not respond, if sent too early (depends on PC computing power).

* Update Nodes - codec can report EAPD capability for certain nodes, but EAPD may not actually physically be there. You want this enabled to update EAPD nodes.

* Sleep Nodes - according to Intel's EAPD handing specifications, EAPD capable nodes have to be suspended properly when machine transitions to sleep .. it's up to you to follow the spec, no harm if it's not done.

### Upon resuming from semi-sleep I loose audio

The only scenario when this can happens is when you have audio playing and suddenly decided you want to put the machine to sleep. If you break out of the it entering sleep you will loose audio until you stop whatever was left playing and allow codec to enter idle. 

## Custom Commands & Commander Client 

You can send the codec your custom commands during boot, upon sleep or at wake. This functionality is part of the customizations coded in by @the-darkvoid to mimic automated hda-verb scripts. CommanderClient (which technically is hda-verb osx clone) is a more adequate tool for experimenting, though - once you polish the command and know it works you can add it to the custom commands section.

The structure of the commands is as follows:


				<key>Custom Commands</key>
				<array>
					<dict>
						<key>Command</key>
						<data>AhcIgw==</data>
						<key>Comment</key>
						<string>0x21 SET_UNSOLICITED_ENABLE 0x83</string>
						<key>On Init</key>
						<true/>
						<key>On Sleep</key>
						<false/>
						<key>On Wake</key>
						<true/>
					</dict>
				</array>

The actual command is specified in any of the plist editors (don't try deciphering base64 as is), be it Xcode or PlistEdit. You can opt to execute the command on cold boot, on sleep and on wake by setting respective flags. 

## Profiles

The easiest way to create profiles, again, is via a proper plist editing tool, opposed to notepad or similar.
			
You have to define a new profile, which is vendorid_deviceid, followed by profile Name

				<key>10ec_0269</key>
				<string>Realtek ALC269</string>

If there's already a profile for your codec, but you have a different variant and that config doesn't suite you, then you can use extended profile definition like this 

				<key>10ec_0269_HDA_1028_04d9</key>
				<string>Realtek ALC269</string>

which uses the subvendor id of your board as well. To know your subvendor you can look in IORegistry or log in Console for the log of CC:

				CodecCommander: Version 2.4.0 starting.
				CodecCommander: ....CodecVendor Id: 0x10ec0269
				CodecCommander: ....Codec Address: 0
				CodecCommander: ....Subsystem Id: 0x102804d9
				CodecCommander: ....PCI Sub Id: 0x102804d9
				
Then, to set up a profile you need to create a dictionary referencing the name you just assigned. 

Default profile is merged with your custom profile, so all you have to do is override the setting that you don't feel like suite your codec with default values configured. 

				<key>Realtek ALC269VB</key>
				<dict>
					<key>Custom Commands</key>
					<array>
						<dict>
							<key>Command</key>
							<data>AhcIgw==</data>
							<key>Comment</key>
							<string>0x21 SET_UNSOLICITED_ENABLE 0x83</string>
							<key>On Init</key>
							<true/>
							<key>On Sleep</key>
							<false/>
							<key>On Wake</key>
							<true/>
						</dict>
					</array>
					<key>Send Delay</key>
					<integer>20</integer>
					<key>Sleep Nodes</key>
					<false/>
				</dict>

## HDMI codecs

By default HDMI codecs re disabled in order to prevent CC attaching on them. If for some reason you feel the need to tamper with HDMI, feel free to remove this restriction.

				<key>8086</key>
				<string>Disabled HDMI</string>
				<key>10de</key>
				<string>Disabled HDMI</string>
				<key>1002</key>
				<string>Disabled HDMI</string>				

 

### Credits

- EAPD fix (resumable-mutable-sound-v1 for IOAudioFamily): km9

- Revamp of the project: RehabMan, the-darkvoid

- Codec Function Group reset at wake idea: EMlyDinEsHMG
