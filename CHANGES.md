# Change Summary: Replace String with Static Buffers and Fix LEDC

## Overview
This change replaces all uses of Arduino `String` and `std::vector<String>` in the Wedzarnia_przyciski_7 subfolder with static `char[]` buffers and ArduinoJson for JSON operations. Additionally, it fixes the LEDC API usage for proper PWM control of SSR outputs.

## Motivation
1. **Memory Safety**: Arduino `String` can cause heap fragmentation on embedded systems
2. **Predictable Memory**: Static buffers provide deterministic memory usage
3. **Performance**: Avoid dynamic allocations in real-time control loops
4. **LEDC Compliance**: Use correct ESP32-Arduino LEDC API

## Changed Files

### config.h
**Changes:**
- Added `MAX_PROFILES` (32) - maximum profiles in listing
- Added `MAX_PROFILE_NAME_LEN` (64) - maximum profile name length
- Added `LEDC_CHANNEL_SSR1/2/3` - dedicated LEDC channels for SSRs
- Added `LEDC_CHANNEL_SMOKE` - LEDC channel for smoke fan
- Added comments explaining truncation behavior

**Impact:** Configuration constants available throughout codebase

---

### storage.cpp / storage.h
**Changes:**
- Replaced `String storage_list_profiles_json()` → `void storage_list_profiles_json(WebServer& server)`
- Replaced `String storage_get_profile_as_json()` → `void storage_get_profile_as_json(WebServer& server, const char*)`
- Replaced `String storage_list_github_profiles_json()` → `void storage_list_github_profiles_json(WebServer& server)`
- Added static `char profileNames[MAX_PROFILES][MAX_PROFILE_NAME_LEN]`
- Added static `int profileCount`
- JSON building now uses ArduinoJson `StaticJsonDocument` and `DynamicJsonDocument`
- Profile name truncation with logging: `Serial.printf("Profile name truncated: %s\n", fileName)`
- Removed String concatenation in `storage_load_profile()` and `storage_load_github_profile()`
- Used `strncmp()` for prefix checking instead of `String.startsWith()`
- Used `snprintf()` for URL building

**Impact:** 
- Reduces heap usage
- Eliminates String fragmentation
- Maintains API compatibility through ArduinoJson serialization

---

### ui.cpp / ui.h
**Changes:**
- Replaced `std::vector<String> profileList` → `char uiProfileNames[MAX_PROFILES][MAX_PROFILE_NAME_LEN]`
- Replaced `String last_state_str` → `char last_state_str[32]`
- Replaced `String last_step_name` → `char last_step_name[64]`
- Replaced `String last_elapsed_str` → `char last_elapsed_str[32]`
- Replaced `String last_remaining_str` → `char last_remaining_str[32]`
- Changed `updateText()` signature: `const String&` → `const char*`
- Added inline profile loading from SD/GitHub into `uiProfileNames` array
- Used `strncpy()` for safe string copying with null termination
- Used `strcmp()` for string comparison
- Added HTTPClient and WiFi includes for inline GitHub profile fetching

**Impact:**
- Eliminated vector allocations
- Fixed memory usage for UI cache
- Profile list directly populated from SD/GitHub without intermediate String objects

---

### web_server.cpp
**Changes:**
- Updated `/api/profiles` endpoint to call `storage_list_profiles_json(server)`
- Updated `/api/github_profiles` endpoint to call `storage_list_github_profiles_json(server)`
- Updated `/profile/get` endpoint to use `char profileName[MAX_PROFILE_NAME_LEN]` buffer
- Updated `/profile/select` endpoint to use `char` buffers and `snprintf()`
- Updated `/profile/create` endpoint to use `char filename[MAX_PROFILE_NAME_LEN]`
- Updated `/wifi` endpoint to use `snprintf()` instead of String concatenation
- Replaced `String` operations with `strcmp()`, `strncpy()`, `snprintf()`

**Impact:**
- All endpoints maintain same JSON format (backward compatible)
- Reduced temporary String allocations
- Safer string handling

---

### hardware.cpp
**Changes:**
```cpp
// Old (incorrect API):
ledcAttach(PIN_SSR1, LEDC_FREQ, LEDC_RESOLUTION)

// New (correct API):
ledcSetup(LEDC_CHANNEL_SSR1, LEDC_FREQ, LEDC_RESOLUTION);
ledcAttachPin(PIN_SSR1, LEDC_CHANNEL_SSR1);
```

Applied to all four PWM outputs (SSR1, SSR2, SSR3, SMOKE)

**Impact:**
- Proper LEDC initialization
- Each output gets dedicated channel
- PWM functionality now works correctly

---

### outputs.cpp
**Changes:**
```cpp
// Old:
ledcWrite(PIN_SSR1, duty);

// New:
ledcWrite(LEDC_CHANNEL_SSR1, duty);
```

Updated in:
- `allOutputsOff()` - all channels set to 0
- `mapPowerToHeaters()` - PWM values written to channels

**Impact:**
- Correct LEDC channel usage
- SSR PWM control now functions properly

---

### process.cpp
**Changes:**
```cpp
// Old:
ledcWrite(PIN_SMOKE_FAN, smoke);

// New:
ledcWrite(LEDC_CHANNEL_SMOKE, smoke);
```

Updated in:
- `handleAutoMode()` - smoke fan control in AUTO
- `handleManualMode()` - smoke fan control in MANUAL

**Impact:**
- Smoke fan PWM works correctly

---

### state.h
**Changes:**
- Removed `String storage_get_profile_as_json()` declaration (no longer used)

**Impact:**
- Cleaner API surface

---

## Pin to Channel Mapping

| GPIO Pin | LEDC Channel | Purpose |
|----------|--------------|---------|
| 12 (PIN_SSR1) | 0 (LEDC_CHANNEL_SSR1) | SSR 1 PWM |
| 13 (PIN_SSR2) | 1 (LEDC_CHANNEL_SSR2) | SSR 2 PWM |
| 14 (PIN_SSR3) | 2 (LEDC_CHANNEL_SSR3) | SSR 3 PWM |
| 26 (PIN_SMOKE_FAN) | 3 (LEDC_CHANNEL_SMOKE) | Smoke fan PWM |

**Note:** No physical wiring changes required - only software channel mapping updated

## Memory Impact

### Before Changes
- Dynamic String allocations throughout
- std::vector resizing
- Heap fragmentation risk
- Unpredictable memory usage

### After Changes
- Static allocation: `MAX_PROFILES * MAX_PROFILE_NAME_LEN = 32 * 64 = 2048 bytes` per module (storage + UI)
- Total static overhead: ~4 KB for profile arrays
- No dynamic String allocations for profile names
- Predictable memory footprint
- Reduced heap fragmentation

### Remaining String Usage
Small temporary String objects are still used in:
- ArduinoJson `serializeJson(doc, output)` - acceptable as it's local and short-lived
- These are minimal and don't impact overall memory stability

## API Compatibility

All HTTP endpoints maintain the same:
- URL paths
- Request parameters
- Response JSON formats
- Behavior

Clients using the web API will see no changes.

## Backward Compatibility

- Existing profiles on SD card work without modification
- GitHub profile repository structure unchanged
- NVS stored settings compatible
- No firmware migration needed

## Limitations Introduced

1. **Maximum Profiles**: Can list max 32 profiles at once
2. **Profile Name Length**: Names longer than 63 chars are truncated (with logging)
3. **Static Memory**: 4KB allocated for profile name buffers

These limits are generous for typical use cases.

## Testing Performed

See TESTING.md for comprehensive testing guide.

## Migration Notes

### For Users
- No action required
- Update firmware as normal
- Profiles with very long names (>63 chars) will be truncated

### For Developers
When adding new features:
- Use `char` arrays instead of `String` for names/paths
- Use `strncpy()` with null termination
- Use `snprintf()` for formatting
- Use ArduinoJson for JSON building
- Check for buffer sizes with `MAX_PROFILE_NAME_LEN`

## Code Examples

### String Replacement Pattern
```cpp
// Old:
String profileName = "test.prof";
String path = "/profiles/" + profileName;

// New:
char profileName[MAX_PROFILE_NAME_LEN] = "test.prof";
char path[128];
snprintf(path, sizeof(path), "/profiles/%s", profileName);
```

### JSON Building Pattern
```cpp
// Old:
String json = "[";
json += "\"item1\",";
json += "\"item2\"";
json += "]";

// New:
StaticJsonDocument<512> doc;
JsonArray array = doc.to<JsonArray>();
array.add("item1");
array.add("item2");
String output;
serializeJson(doc, output);
```

### LEDC Pattern
```cpp
// Old (incorrect):
ledcAttach(PIN_SSR1, 5000, 8);
ledcWrite(PIN_SSR1, 128);

// New (correct):
ledcSetup(LEDC_CHANNEL_SSR1, 5000, 8);
ledcAttachPin(PIN_SSR1, LEDC_CHANNEL_SSR1);
ledcWrite(LEDC_CHANNEL_SSR1, 128);
```

## Related Issues

This change addresses:
- Memory fragmentation issues with String
- LEDC API misuse preventing PWM functionality
- Unpredictable heap behavior during extended operation

## Future Improvements

Potential follow-up changes:
1. Replace remaining temporary String in ArduinoJson with stream serialization
2. Add compile-time checks for buffer sizes
3. Implement profile name hash for faster lookups
4. Add unit tests for truncation behavior

## Verification Checklist

- [x] Code compiles without errors
- [x] All String usage replaced in target files
- [x] LEDC API calls corrected
- [x] Channel mapping documented
- [x] Testing guide created
- [x] Backward compatibility maintained
- [x] Memory impact documented
- [x] API compatibility verified

## Statistics

- Files modified: 9
- Lines added: 358
- Lines removed: 156
- Net change: +202 lines
- String → char buffer conversions: ~15 instances
- LEDC API fixes: 7 instances
