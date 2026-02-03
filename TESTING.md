# Testing Guide for String Replacement Changes

## Overview
This document describes how to test the changes made to replace Arduino `String` and `std::vector<String>` with static `char[]` buffers and ArduinoJson, as well as the LEDC API fixes.

## Prerequisites
- ESP32 development board
- Arduino IDE or PlatformIO
- SD card with test profiles in `/profiles/` directory
- WiFi network access for GitHub profile testing

## Configuration Constants
The following new constants have been added to `config.h`:
- `MAX_PROFILES` (32) - Maximum number of profiles that can be listed
- `MAX_PROFILE_NAME_LEN` (64) - Maximum length of profile names (including .prof extension)
- `LEDC_CHANNEL_SSR1` (0) - LEDC channel for SSR1
- `LEDC_CHANNEL_SSR2` (1) - LEDC channel for SSR2
- `LEDC_CHANNEL_SSR3` (2) - LEDC channel for SSR3
- `LEDC_CHANNEL_SMOKE` (3) - LEDC channel for smoke fan

## Test Cases

### 1. Compilation Test
**Goal:** Verify the code compiles without errors

**Steps:**
1. Open the project in Arduino IDE or PlatformIO
2. Select ESP32 board
3. Compile the project
4. Verify no compilation errors

**Expected Result:** Clean compilation with no errors

---

### 2. SD Card Profile Listing Test
**Goal:** Verify profiles can be listed from SD card

**Setup:**
1. Create test profiles on SD card in `/profiles/` directory:
   - `test1.prof`
   - `test2.prof`
   - `very_long_profile_name_that_exceeds_64_characters_limit_test.prof`

**Steps:**
1. Power on the device
2. Access web interface at device IP
3. Select "Karta SD" as profile source
4. Click refresh or check profile list

**Expected Result:**
- Profiles `test1.prof` and `test2.prof` appear in list
- Long profile name is truncated to 63 characters + null terminator
- Serial monitor shows: "Profile name truncated: very_long_profile_name_that_exceeds_64_characters_limit_test.prof"
- No memory allocation errors

---

### 3. Profile Loading from SD Test
**Goal:** Verify profiles load correctly from SD card

**Steps:**
1. Select a profile from SD card list
2. Click "Ustaw jako aktywny" (Set as active)
3. Start AUTO mode
4. Monitor process execution

**Expected Result:**
- Profile loads successfully
- Steps execute in correct order
- Display shows correct step information
- No crashes or memory errors

---

### 4. GitHub Profile Listing Test
**Goal:** Verify profiles can be listed from GitHub

**Prerequisites:**
- Device connected to WiFi
- GitHub repository configured in `CFG_GITHUB_API_URL`

**Steps:**
1. Access web interface
2. Select "GitHub" as profile source
3. Wait for profile list to load

**Expected Result:**
- GitHub profiles appear in list
- Long profile names are truncated with logging
- Error handling works if WiFi disconnected
- No memory leaks

---

### 5. GitHub Profile Loading Test
**Goal:** Verify profiles load correctly from GitHub

**Steps:**
1. Select a GitHub profile
2. Click "Ustaw jako aktywny"
3. Start AUTO mode

**Expected Result:**
- Profile downloads and loads
- Process starts correctly
- No memory issues

---

### 6. Web API Endpoint Tests
**Goal:** Verify JSON API endpoints work correctly

**Endpoints to test:**

#### `/api/profiles` (GET)
```bash
curl http://<device-ip>/api/profiles
```
**Expected:** JSON array of profile names from SD card

#### `/api/github_profiles` (GET)
```bash
curl http://<device-ip>/api/github_profiles
```
**Expected:** JSON array of profile names from GitHub

#### `/profile/get?name=test1.prof&source=sd` (GET)
```bash
curl "http://<device-ip>/profile/get?name=test1.prof&source=sd"
```
**Expected:** JSON array of profile steps

#### `/status` (GET)
```bash
curl http://<device-ip>/status
```
**Expected:** JSON with current system status

**All endpoints should:**
- Return valid JSON
- Maintain backward compatibility with previous API format
- Not cause memory leaks

---

### 7. LEDC/PWM Control Test
**Goal:** Verify SSR and smoke fan PWM control works correctly

**Hardware Required:**
- Multimeter or oscilloscope
- LED or SSR connected to GPIO 12, 13, 14 (SSR1/2/3)
- Fan or LED connected to GPIO 26 (smoke fan)

**Steps:**
1. Start MANUAL mode
2. Set power mode to 1, 2, and 3
3. Measure PWM output on SSR pins
4. Adjust smoke PWM from 0 to 255
5. Measure PWM on smoke fan pin

**Expected Result:**
- SSR1 (GPIO 12, channel 0) - correct PWM output
- SSR2 (GPIO 13, channel 1) - correct PWM output
- SSR3 (GPIO 14, channel 2) - correct PWM output
- Smoke fan (GPIO 26, channel 3) - correct PWM output
- Different power modes activate different number of SSRs
- PWM duty cycle corresponds to set values

**Measurements:**
- Frequency: 5000 Hz
- Resolution: 8-bit (0-255)
- Power mode 1: Only SSR1 active
- Power mode 2: SSR1 and SSR2 active
- Power mode 3: All SSRs active

---

### 8. UI Display Test
**Goal:** Verify LCD display works correctly with char buffers

**Steps:**
1. Navigate through all UI screens:
   - Main dashboard
   - Main menu
   - Source selection menu
   - Profile list
   - Manual edit screen
   - Confirmation screen
2. Check temperature displays
3. Check timer displays
4. Check profile name display

**Expected Result:**
- All text displays correctly
- No garbled characters
- Temperature values update smoothly
- Timers count correctly
- Profile names display without truncation (if < 64 chars)

---

### 9. Memory Stress Test
**Goal:** Verify no memory leaks or fragmentation

**Steps:**
1. Enable serial monitor
2. Run device for extended period (1+ hours)
3. Perform multiple operations:
   - List profiles repeatedly (both SD and GitHub)
   - Load different profiles
   - Switch between AUTO and MANUAL modes
   - Access web interface multiple times

**Monitor:**
```
[HEAP] Free: XXXXX B, Min: XXXXX B
```

**Expected Result:**
- Free heap stays stable or decreases minimally
- No "LOW MEMORY WARNING" messages
- No crashes or reboots
- Min heap doesn't decrease significantly over time

---

### 10. Long Profile Name Test
**Goal:** Verify profile name truncation works correctly

**Steps:**
1. Create profile with name exactly 64 characters: `profile_name_exactly_64_characters_including_extension_ab.prof`
2. Create profile with name > 64 characters
3. List profiles from SD and GitHub

**Expected Result:**
- 64-character name: displays correctly (63 chars + null)
- Longer names: truncated to 63 characters + null terminator
- Serial log shows truncation message
- No buffer overflows
- No crashes

---

## Performance Benchmarks

### Memory Usage
Before changes (with String):
- Heap usage: ~XXX KB (varies)
- String fragmentation: possible

After changes (with char buffers):
- Heap usage: Should be more stable
- No String fragmentation
- Static allocation: MAX_PROFILES * MAX_PROFILE_NAME_LEN = 32 * 64 = 2KB

### Known Limitations
1. Maximum 32 profiles can be listed at once (MAX_PROFILES)
2. Profile names longer than 63 characters will be truncated
3. Some temporary String objects still used in ArduinoJson serialization (acceptable)

## Debugging

### Common Issues

**Issue:** Compilation error about WebServer parameter
**Solution:** Ensure storage.h includes `<WebServer.h>`

**Issue:** Profile names appear garbled
**Solution:** Check for buffer overflows, ensure null termination

**Issue:** LEDC not working
**Solution:** Verify LEDC channels don't conflict with other uses

**Issue:** Memory warnings
**Solution:** Check for unclosed File handles or HTTP connections

### Serial Monitor Output

Expected log messages:
```
=== WEDZARNIA ESP32 v3.0 (Modular) ===
...
LEDC initialized for SSR1, SSR2, SSR3, and SMOKE
...
Znalezione profile: X
Profile loaded from SD: Y steps
```

With long names:
```
Profile name truncated: very_long_profile_name...
```

## Success Criteria

All tests must pass with:
- ✅ No compilation errors
- ✅ No runtime crashes
- ✅ No memory leaks
- ✅ All features working as before
- ✅ LEDC channels properly configured
- ✅ Profile name truncation working with logging
- ✅ API endpoints returning correct JSON
- ✅ UI displaying correctly

## Additional Notes

- ArduinoJson library must be installed
- ESP32 Arduino core must support ledcSetup/ledcAttachPin/ledcWrite
- The changes maintain backward compatibility with existing profiles
- No changes to physical wiring required - only channel mapping updated
