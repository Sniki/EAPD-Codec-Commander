/*
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include "Configuration.h"

// Constants for Configuration
#define kDefault                    "Default"
#define kPerformReset               "Perform Reset"
#define kPerformResetOnExternalWake "Perform Reset on External Wake"
#define kPerformResetOnEAPDFail     "Perform Reset on EAPD Fail"
#define kCodecId                    "Codec Id"
#define kDisable                    "Disable"
#define kCodecAddressMask           "CodecAddressMask"

// Constants for EAPD command verb sending
#define kUpdateNodes                "Update Nodes"
#define kSleepNodes                 "Sleep Nodes"
#define kSendDelay                  "Send Delay"

// Workloop required and Workloop timer aka update interval, ms
#define kCheckInfinitely            "Check Infinitely"
#define kCheckInterval              "Check Interval"

// Constants for custom commands
#define kCustomCommands             "Custom Commands"
#define kCustomCommand              "Command"
#define kCommandOnInit              "On Init"
#define kCommandOnSleep             "On Sleep"
#define kCommandOnWake              "On Wake"
#define kCommandLayoutID            "LayoutID"

// Constants for Pin Configuration
#define kPinConfigDefault           "PinConfigDefault"

// Parsing for configuration

UInt32 Configuration::parseInteger(const char* str)
{
    UInt32 result = 0;
    while (*str == ' ') ++str;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        str += 2;
        while (*str)
        {
            result <<= 4;
            if (*str >= '0' && *str <= '9')
                result |= *str - '0';
            else if (*str >= 'A' && *str <= 'F')
                result |= *str - 'A' + 10;
            else if (*str >= 'a' && *str <= 'f')
                result |= *str - 'a' + 10;
            else
                return 0;
            ++str;
        }
    }
    else
    {
        while (*str)
        {
            result *= 10;
            if (*str >= '0' && *str <= '9')
                result += *str - '0';
            else
                return 0;
            ++str;
        }
    }
    return result;
}

bool Configuration::getBoolValue(OSDictionary *dict, const char *key, bool defValue)
{
    bool result = defValue;
    if (dict)
    {
        if (OSBoolean* bl = OSDynamicCast(OSBoolean, dict->getObject(key)))
            result = bl->getValue();
    }
    return result;
}

UInt32 Configuration::getIntegerValue(OSDictionary *dict, const char *key, UInt32 defValue)
{
    UInt32 result = defValue;
    if (dict)
        result = getIntegerValue(dict->getObject(key), defValue);
    return result;
}

UInt32 Configuration::getIntegerValue(OSObject *obj, UInt32 defValue)
{
    UInt32 result = defValue;
    if (OSNumber* num = OSDynamicCast(OSNumber, obj))
        result = num->unsigned32BitValue();
    else if (OSString* str = OSDynamicCast(OSString, obj))
        result = parseInteger(str->getCStringNoCopy());
    return result;
}

OSDictionary* Configuration::locateConfiguration(OSDictionary* profiles, UInt32 codecVendorId, UInt32 subsystemId)
{
    UInt16 vendor = codecVendorId >> 16;
    UInt16 codec = codecVendorId & 0xFFFF;
    OSObject* obj;

    // check vendor_codec_HDA_full-subsystem first
    char codecLookup[sizeof("vvvv_cccc_HDA_xxxx_dddd")];
    snprintf(codecLookup, sizeof(codecLookup), "%04x_%04x_HDA_%04x_%04x", vendor, codec, subsystemId >> 16, subsystemId & 0xFFFF);
    obj = profiles->getObject(codecLookup);
    if (!obj)
    {
        // check vendor_codec_HDA_vendorsubid next
        snprintf(codecLookup, sizeof(codecLookup), "%04x_%04x_HDA_%04x", vendor, codec, subsystemId >> 16);
        obj = profiles->getObject(codecLookup);
        if (!obj)
        {
            // check vendor_codec next
            snprintf(codecLookup, sizeof(codecLookup), "%04x_%04x", vendor, codec);
            obj = profiles->getObject(codecLookup);
            if (!obj)
            {
                // not found, check for vendor override (used for Intel HDMI)
                snprintf(codecLookup, sizeof(codecLookup), "%04x", vendor);
                obj = profiles->getObject(codecLookup);
            }
        }
    }

    // look up actual dictionary (can be string redirect)
    OSDictionary* dict;
    if (OSString* str = OSDynamicCast(OSString, obj))
        dict = OSDynamicCast(OSDictionary, profiles->getObject(str));
    else
        dict = OSDynamicCast(OSDictionary, obj);
    
    return dict;
}

OSObject* Configuration::translateEntry(OSObject* obj)
{
    // Note: non-NULL result is retained...

    // if object is another array, translate it
    if (OSArray* array = OSDynamicCast(OSArray, obj))
        return translateArray(array);

    // if object is a string, may be translated to boolean
    if (OSString* string = OSDynamicCast(OSString, obj))
    {
        // object is string, translate special boolean values
        const char* sz = string->getCStringNoCopy();
        if (sz[0] == '>')
        {
            // boolean types true/false
            if (sz[1] == 'y' && !sz[2])
                return OSBoolean::withBoolean(true);
            else if (sz[1] == 'n' && !sz[2])
                return OSBoolean::withBoolean(false);
            // escape case ('>>n' '>>y'), replace with just string '>n' '>y'
            else if (sz[1] == '>' && (sz[2] == 'y' || sz[2] == 'n') && !sz[3])
                return OSString::withCString(&sz[1]);
        }
    }
    return NULL; // no translation
}

OSObject* Configuration::translateArray(OSArray* array)
{
    // may return either OSArray* or OSDictionary*

    int count = array->getCount();
    if (!count)
        return NULL;

    OSObject* result = array;

    // if first entry is an empty array, process as array, else dictionary
    OSArray* test = OSDynamicCast(OSArray, array->getObject(0));
    if (test && test->getCount() == 0)
    {
        // using same array, but translating it...
        array->retain();

        // remove bogus first entry
        array->removeObject(0);
        --count;

        // translate entries in the array
        for (int i = 0; i < count; ++i)
        {
            if (OSObject* obj = translateEntry(array->getObject(i)))
            {
                array->replaceObject(i, obj);
                obj->release();
            }
        }
    }
    else
    {
        // array is key/value pairs, so must be even
        if (count & 1)
            return NULL;

        // dictionary constructed to accomodate all pairs
        int size = count >> 1;
        if (!size) size = 1;
        OSDictionary* dict = OSDictionary::withCapacity(size);
        if (!dict)
            return NULL;

        // go through each entry two at a time, building the dictionary
        for (int i = 0; i < count; i += 2)
        {
            OSString* key = OSDynamicCast(OSString, array->getObject(i));
            if (!key)
            {
                dict->release();
                return NULL;
            }
            // get value, use translated value if translated
            OSObject* obj = array->getObject(i+1);
            OSObject* trans = translateEntry(obj);
            if (trans)
                obj = trans;
            dict->setObject(key, obj);
            OSSafeReleaseNULL(trans);
        }
        result = dict;
    }

    // Note: result is retained when returned...
    return result;
}

OSDictionary* Configuration::getConfigurationOverride(const char* method, IOService* provider, const char* name)
{
    OSDictionary* dict = OSDynamicCast(OSDictionary, provider->getProperty(kRMCFCache));
    if (!dict)
    {
        // find associated ACPI device
        OSString* acpiPath = OSDynamicCast(OSString, provider->getProperty("acpi-path"));
        if (!acpiPath)
            return NULL;
        IOACPIPlatformDevice* acpi = OSDynamicCast(IOACPIPlatformDevice, IOACPIPlatformDevice::fromPath(acpiPath->getCStringNoCopy()));
        if (!acpi)
            return NULL;

        // attempt to get configuration data from provider
        OSObject* r = NULL;
        if (kIOReturnSuccess != acpi->evaluateObject(method, &r))
            return NULL;

        // for translation method must return array
        OSObject* obj = NULL;
        OSArray* array = OSDynamicCast(OSArray, r);
#ifdef DEBUG
        if (array)
        {
            OSCollection* copy = array->copyCollection();
            if (copy)
                provider->setProperty("RMCF.result", copy);
        }
#endif
        if (array)
            obj = translateArray(array);
        OSSafeReleaseNULL(r);

        // must be dictionary after translation, even though array is possible
        dict = OSDynamicCast(OSDictionary, obj);
        if (!dict)
        {
            OSSafeReleaseNULL(obj);
            return NULL;
        }
        provider->setProperty(kRMCFCache, dict);
        dict->release();    // dict is retained by setProperty, still valid
    }

    // actual configuration is in subproperty/dictionary
    OSDictionary* result = OSDynamicCast(OSDictionary, dict->getObject(name));
    return result;
}

OSDictionary* Configuration::loadConfiguration(OSDictionary* profiles, UInt32 codecVendorId, UInt32 subsystemId)
{
    OSDictionary* defaultProfile = NULL;
    OSDictionary* codecProfile = NULL;
    if (profiles)
    {
        defaultProfile = OSDynamicCast(OSDictionary, profiles->getObject(kDefault));
        codecProfile = locateConfiguration(profiles, codecVendorId, subsystemId);
    }
    OSDictionary* result = NULL;

    if (defaultProfile)
    {
        // have default node, result is merged with platform node
        result = OSDictionary::withDictionary(defaultProfile);
        if (result && codecProfile)
            result->merge(codecProfile);
    }

    if (!result)
    {
        if (codecProfile)
            // no default node, try to use just the codec profile
            result = OSDictionary::withDictionary(codecProfile);
        if (!result)
            // empty dictionary in case of errors/memory problems
            result = OSDictionary::withCapacity(0);
    }

    return result;
}

Configuration::Configuration(OSObject* codecProfiles, IntelHDA* intelHDA, const char* name)
{
    OSDictionary* profiles = OSDynamicCast(OSDictionary, codecProfiles);
    UInt32 codecVendorId = intelHDA->getCodecVendorId();
    UInt32 hdaSubsystemId = intelHDA->getSubsystemId();

    // Load/merge override from RMCF if available
    OSDictionary* custom = getConfigurationOverride("RMCF", intelHDA->getPCIDevice(), name);
    if (custom && profiles)
    {
        if (OSNumber* num = OSDynamicCast(OSNumber, custom->getObject("Version")))
        {
            if (num->unsigned32BitValue() == 0x020600) // version must be marked 0x020600 for new way
            {
                // new way custom configuration (merge into master profile)
                profiles = OSDynamicCast(OSDictionary, profiles->copyCollection());
                if (profiles)
                    profiles->merge(custom);
                custom = NULL;
            }
        }
    }

    // Retrieve platform profile configuration
    OSDictionary* config = loadConfiguration(profiles, codecVendorId, hdaSubsystemId);
    if (profiles != codecProfiles)
        OSSafeReleaseNULL(profiles);

    // old way custom configuration (merge into device specific)
    if (custom)
        config->merge(custom);

#ifdef DEBUG
    mMergedConfig = config;
    if (mMergedConfig)
        mMergedConfig->retain();
#endif

    mCustomCommands = NULL;
    mPinConfigDefault = NULL;

    // if Disable is set in the profile, no more config is gathered, start will fail
    mDisable = getBoolValue(config, kDisable, false);
    if (mDisable)
    {
        OSSafeReleaseNULL(config);
        return;
    }

    // Get CodecAddressMask
    mCodecAddressMask = getIntegerValue(config, kCodecAddressMask, 1);

    // Get delay for sending the verb
    mSendDelay = getIntegerValue(config, kSendDelay, 300);

    // auto detect AppleHDA to turn off the "Perform Reset*" options
    // normally they are default true, but not if AppleALC is used
    mPerformReset = true;
    mPerformResetOnExternalWake = true;
    if (IORegistryEntry* entry = IORegistryEntry::fromPath("IOService:/IOResources/AppleALC"))
    {
        mPerformReset = mPerformResetOnExternalWake = false;
        entry->release();
    }
    mPerformReset = getBoolValue(config, kPerformReset, mPerformReset);
    mPerformResetOnExternalWake = getBoolValue(config, kPerformResetOnExternalWake, mPerformResetOnExternalWake);

    // Determine if perform reset is requested (Defaults to true)
    mPerformResetOnEAPDFail = getBoolValue(config, kPerformResetOnEAPDFail, true);

    // Determine if update to EAPD nodes is requested (Defaults to true)
    mUpdateNodes = getBoolValue(config, kUpdateNodes, true);
    mSleepNodes = getBoolValue(config, kSleepNodes, true);

    // Determine if infinite check is needed (for 10.9 and up)
    mCheckInfinite = getBoolValue(config, kCheckInfinitely, false);
    mCheckInterval = getIntegerValue(config, kCheckInterval, 1000);

    // load PinConfigDefault
    if (config)
    {
        if (OSArray* pinConfig = OSDynamicCast(OSArray, config->getObject(kPinConfigDefault)))
            mPinConfigDefault = (OSArray*)pinConfig->copyCollection();
    }

    mCustomCommands = OSArray::withCapacity(0);
    if (!mCustomCommands)
    {
        OSSafeReleaseNULL(config);
        return;
    }

    // Parse custom commands
    OSArray* list;
    if (config && (list = OSDynamicCast(OSArray, config->getObject(kCustomCommands))))
    {
        unsigned count = list->getCount();
        for (unsigned i = 0; i < count; i++)
        {
            OSDictionary* dict = (OSDictionary*)list->getObject(i);
            OSObject* obj = dict->getObject(kCustomCommand);
            OSData* commandData = NULL;
            CustomCommand* customCommand;

            if (UInt32 commandBits = getIntegerValue(obj, 0))
            {
                commandData = OSData::withCapacity(sizeof(CustomCommand)+sizeof(UInt32));
                if (!commandData)
                    break;
                commandData->appendByte(0, commandData->getCapacity());
                customCommand = (CustomCommand*)commandData->getBytesNoCopy();
                customCommand->CommandCount = 1;
                customCommand->Commands[0] = commandBits;
            }
            else if (OSData* data = OSDynamicCast(OSData, obj))
            {
                unsigned length = data->getLength();
                commandData = OSData::withCapacity(sizeof(CustomCommand)+length);
                if (!commandData)
                    break;
                commandData->appendByte(0, commandData->getCapacity());
                customCommand = (CustomCommand*)commandData->getBytesNoCopy();
                customCommand->CommandCount = length / sizeof(customCommand->Commands[0]);
                // byte reverse here, so the author of Info.pist doesn't have to...
                UInt8* bytes = (UInt8*)data->getBytesNoCopy();
                for (int i = 0; i < customCommand->CommandCount; i++)
                {
                    customCommand->Commands[i] = bytes[0]<<24 | bytes[1]<<16 | bytes[2]<<8 | bytes[3];
                    bytes += sizeof(UInt32);
                }
            }
            if (commandData)
            {
                customCommand->OnInit = getBoolValue(dict, kCommandOnInit, false);
                customCommand->OnSleep = getBoolValue(dict, kCommandOnSleep, false);
                customCommand->OnWake = getBoolValue(dict, kCommandOnWake, false);
                customCommand->layoutID = getIntegerValue(dict, kCommandLayoutID, -1);
                mCustomCommands->setObject(commandData);
                commandData->release();
            }
        }
    }

    OSSafeReleaseNULL(config);

    // Dump parsed configuration
    DebugLog("Configuration\n");
    DebugLog("...Check Infinite: %s\n", mCheckInfinite ? "true" : "false");
    DebugLog("...Check Interval: %d\n", mCheckInterval);
    DebugLog("...Perform Reset: %s\n", mPerformReset ? "true" : "false");
    DebugLog("...Perform Reset on External Wake: %s\n", mPerformResetOnExternalWake ? "true" : "false");
    DebugLog("...Perform Reset on EAPD Fail: %s\n", mPerformResetOnEAPDFail ? "true" : "false");
    DebugLog("...Send Delay: %d\n", mSendDelay);
    DebugLog("...Update Nodes: %s\n", mUpdateNodes ? "true" : "false");
    DebugLog("...Sleep Nodes: %s\n", mSleepNodes ? "true" : "false");

#ifdef DEBUG
    if (mCustomCommands)
    {
        unsigned count = mCustomCommands->getCount();
        for (unsigned i = 0; i < count; i++)
        {
            OSData* data = (OSData*)mCustomCommands->getObject(i);
            CustomCommand* customCommand = (CustomCommand*)data->getBytesNoCopy();
            DebugLog("Custom Command\n");
            if (customCommand->CommandCount == 1)
                DebugLog("...Command: 0x%08x\n", customCommand->Commands[0]);
            if (customCommand->CommandCount == 2)
                DebugLog("...Commands(%d): 0x%08x 0x%08x\n", customCommand->CommandCount, customCommand->Commands[0], customCommand->Commands[1]);
            if (customCommand->CommandCount == 3)
                DebugLog("...Commands(%d): 0x%08x 0x%08x 0x%08x\n", customCommand->CommandCount, customCommand->Commands[0], customCommand->Commands[1], customCommand->Commands[2]);
            if (customCommand->CommandCount > 3)
                DebugLog("...Commands(%d): 0x%08x 0x%08x 0x%08x 0x%08x ...\n", customCommand->CommandCount, customCommand->Commands[0], customCommand->Commands[1], customCommand->Commands[2], customCommand->Commands[3]);
            DebugLog("...OnInit: %s\n", customCommand->OnInit ? "true" : "false");
            DebugLog("...OnWake: %s\n", customCommand->OnWake ? "true" : "false");
            DebugLog("...OnSleep: %s\n", customCommand->OnSleep ? "true" : "false");
            DebugLog("...LayoutID: %d\n", customCommand->layoutID);
        }
    }
#endif
}

Configuration::~Configuration()
{
#ifdef DEBUG
    OSSafeReleaseNULL(mMergedConfig);
#endif
    OSSafeReleaseNULL(mPinConfigDefault);
    OSSafeReleaseNULL(mCustomCommands);
}

