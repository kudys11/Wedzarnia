# PR Review Guide: String Replacement & LEDC Fix

## Quick Start for Reviewers

This PR replaces Arduino `String` with static `char[]` buffers and fixes LEDC API usage. Here's how to review it effectively.

## Review Order (Recommended)

1. **CHANGES.md** (5 min) - Get overview of all changes
2. **config.h** (2 min) - Check new constants
3. **storage.cpp/h** (10 min) - Core storage refactoring
4. **ui.cpp** (10 min) - UI buffer changes
5. **web_server.cpp** (8 min) - API endpoint updates
6. **hardware.cpp, outputs.cpp, process.cpp** (5 min) - LEDC fixes
7. **TESTING.md** (5 min) - Review test plan

**Total review time: ~45 minutes**

## Key Changes to Focus On

### 1. String → char[] Pattern
Look for these patterns throughout:

```cpp
// OLD pattern (removed)
String profileName = "test.prof";
String path = "/profiles/" + profileName;

// NEW pattern (implemented)
char profileName[MAX_PROFILE_NAME_LEN];
strncpy(profileName, "test.prof", sizeof(profileName) - 1);
profileName[sizeof(profileName) - 1] = '\0';  // Always null-terminate

char path[128];
snprintf(path, sizeof(path), "/profiles/%s", profileName);
```

**✅ Check for:**
- Null termination after strncpy()
- Buffer size checks
- No buffer overflows

### 2. JSON Building with ArduinoJson
```cpp
// OLD (String concatenation)
String json = "[\"item1\",\"item2\"]";

// NEW (ArduinoJson)
StaticJsonDocument<512> doc;
JsonArray array = doc.to<JsonArray>();
array.add("item1");
array.add("item2");
String output;
serializeJson(doc, output);
server.send(200, "application/json", output);
```

**✅ Check for:**
- Appropriate document sizes (StaticJsonDocument vs DynamicJsonDocument)
- Proper serialization to WebServer
- No memory leaks from unclosed HTTP connections

### 3. LEDC API Fix
```cpp
// OLD (incorrect API)
ledcAttach(PIN_SSR1, LEDC_FREQ, LEDC_RESOLUTION);
ledcWrite(PIN_SSR1, duty);

// NEW (correct API)
ledcSetup(LEDC_CHANNEL_SSR1, LEDC_FREQ, LEDC_RESOLUTION);
ledcAttachPin(PIN_SSR1, LEDC_CHANNEL_SSR1);
ledcWrite(LEDC_CHANNEL_SSR1, duty);
```

**✅ Check for:**
- All ledcWrite calls use LEDC_CHANNEL_* not PIN_*
- Each output has unique channel (0-3)
- No channel conflicts

## Critical Review Points

### Memory Safety
**Check for these anti-patterns:**
- [ ] strcpy() without size limit → Should be strncpy()
- [ ] Missing null terminator after strncpy()
- [ ] Buffer sizes too small for content
- [ ] Unclosed File handles
- [ ] Unclosed HTTP connections

**Verify these patterns:**
- [x] All string copies use strncpy() with size-1
- [x] All strncpy() followed by null terminator
- [x] Buffer sizes defined by MAX_PROFILE_NAME_LEN
- [x] File.close() in all code paths
- [x] http.end() after all HTTP requests

### API Compatibility
**Endpoints to verify:**
```bash
GET /api/profiles              # Should return JSON array
GET /api/github_profiles       # Should return JSON array
GET /profile/get?name=X&source=Y  # Should return profile JSON
GET /status                    # Should return status JSON
```

**✅ Verify:**
- [ ] JSON format unchanged
- [ ] Response structure identical to before
- [ ] Error handling maintained

### LEDC Channel Mapping
**Verify correct mapping:**
```cpp
// In config.h
#define LEDC_CHANNEL_SSR1 0    // GPIO 12
#define LEDC_CHANNEL_SSR2 1    // GPIO 13
#define LEDC_CHANNEL_SSR3 2    // GPIO 14
#define LEDC_CHANNEL_SMOKE 3   // GPIO 26
```

**✅ Check:**
- [ ] No duplicate channels
- [ ] Channels 0-3 used (4-15 available for future)
- [ ] All ledcWrite calls updated

## Code Smells to Watch For

### ❌ Bad Patterns (should NOT be present)
```cpp
// Don't use strcpy (unbounded)
strcpy(dest, src);

// Don't forget null terminator
strncpy(dest, src, size);  // Missing dest[size-1] = '\0';

// Don't use String for profile names
String profileName;

// Don't use PIN_* with ledcWrite
ledcWrite(PIN_SSR1, duty);
```

### ✅ Good Patterns (should BE present)
```cpp
// Safe string copy
strncpy(dest, src, sizeof(dest) - 1);
dest[sizeof(dest) - 1] = '\0';

// Format strings safely  
snprintf(buf, sizeof(buf), "format %s", str);

// ArduinoJson for JSON
StaticJsonDocument<512> doc;
JsonArray arr = doc.to<JsonArray>();

// Correct LEDC usage
ledcWrite(LEDC_CHANNEL_SSR1, duty);
```

## Testing Checklist

Before approving, verify these work:

### Functional Tests
- [ ] Code compiles without errors
- [ ] List profiles from SD card
- [ ] List profiles from GitHub
- [ ] Load profile from SD
- [ ] Load profile from GitHub
- [ ] Web interface displays profiles correctly
- [ ] Long profile names are truncated (check serial log)

### Hardware Tests (if available)
- [ ] SSR1 PWM works (GPIO 12, channel 0)
- [ ] SSR2 PWM works (GPIO 13, channel 1)
- [ ] SSR3 PWM works (GPIO 14, channel 2)
- [ ] Smoke fan PWM works (GPIO 26, channel 3)
- [ ] Different power modes activate correct SSRs

### Memory Tests
- [ ] Run for 1+ hour
- [ ] Monitor serial for heap stats
- [ ] No "LOW MEMORY WARNING"
- [ ] Heap remains stable

## Quick Verification Commands

If you have the hardware:

```bash
# List SD profiles
curl http://<ip>/api/profiles

# List GitHub profiles  
curl http://<ip>/api/github_profiles

# Get profile details
curl "http://<ip>/profile/get?name=test.prof&source=sd"

# Check status
curl http://<ip>/status
```

## Common Issues to Check

### Issue: Compilation errors about WebServer
**Solution**: storage.h should include `<WebServer.h>`

### Issue: Buffer overflow warnings
**Solution**: Check all strncpy() sizes and null terminators

### Issue: JSON format changed
**Problem**: API compatibility broken
**Solution**: Verify ArduinoJson creates same structure

### Issue: PWM not working
**Problem**: LEDC channels not properly configured
**Solution**: Check ledcSetup() + ledcAttachPin() + ledcWrite(channel)

## Files Size Check

Expected changes per `git diff --stat`:
```
Wedzarnia_przyciski_7/config.h       |   9 ++++
Wedzarnia_przyciski_7/hardware.cpp   |  21 +++++----
Wedzarnia_przyciski_7/outputs.cpp    |  14 +++---
Wedzarnia_przyciski_7/process.cpp    |   4 +-
Wedzarnia_przyciski_7/state.h        |   1 -
Wedzarnia_przyciski_7/storage.cpp    | 189 +++++++++++++++--------
Wedzarnia_przyciski_7/storage.h      |  19 ++++----
Wedzarnia_przyciski_7/ui.cpp         | 159 ++++++++++++++-----
Wedzarnia_przyciski_7/web_server.cpp |  98 +++++++++---
9 files changed, 358 insertions(+), 156 deletions(-)
```

## Sign-off Checklist

Before approving this PR, confirm:

- [ ] Reviewed CHANGES.md
- [ ] Reviewed key code changes in storage.cpp
- [ ] Checked LEDC channel mapping
- [ ] Verified no buffer overflows possible
- [ ] Confirmed API compatibility
- [ ] Reviewed testing plan in TESTING.md
- [ ] Code compiles (if tested locally)
- [ ] Memory usage acceptable (~4KB static overhead)

## Approval Criteria

✅ **Approve if:**
- All buffer operations are safe (strncpy + null terminator)
- LEDC channels properly mapped
- API responses match original format
- Code compiles successfully
- Documentation is comprehensive

❌ **Request changes if:**
- Buffer overflow risks exist
- Missing null terminators
- API compatibility broken
- LEDC channels conflict
- Compilation errors

## Questions for PR Author

If you have concerns, ask about:
1. Why ArduinoJson instead of pure char[] for JSON?
2. Is 32 profile limit sufficient?
3. Is 64 character name limit reasonable?
4. Why these specific LEDC channels (0-3)?
5. Performance impact of ArduinoJson?

## Additional Resources

- **ArduinoJson docs**: https://arduinojson.org/
- **ESP32 LEDC API**: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html
- **String best practices**: Avoid on ESP32 due to heap fragmentation

---

**Happy reviewing! 🎉**

If you find any issues, please comment on specific lines or ask questions in the PR discussion.
