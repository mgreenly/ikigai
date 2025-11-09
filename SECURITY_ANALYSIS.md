# Input Parser Security Analysis

## Executive Summary

I conducted a security-focused analysis of the input parser module (`src/input.c`) using a hacker mindset to identify potential vulnerabilities. The parser was subjected to 14 pathological test cases designed to break it through malformed UTF-8, escape sequence abuse, and state confusion attacks.

**Result**: Several **CRITICAL SECURITY VULNERABILITIES** were identified in the UTF-8 decoder.

---

## Critical Vulnerabilities Found

### 1. UTF-8 Overlong Encoding Acceptance ⚠️ CRITICAL

**Severity**: High (Security Bypass Risk)

**Description**: The parser accepts UTF-8 overlong encodings, which is a well-known security vulnerability used to bypass input validation filters.

**Attack Vector**:
```
Normal encoding of 'A' (U+0041):    0x41
Overlong 2-byte encoding:           0xC1 0x81  ← ACCEPTED BY PARSER
Overlong 3-byte encoding of '/':    0xE0 0x80 0xAF  ← ACCEPTED BY PARSER
```

**Why This Matters**:
- Overlong encodings have been used in directory traversal attacks (e.g., encoding '/' as 0xE0 0x80 0xAF)
- Can bypass security filters that only check normal UTF-8 encodings
- Violates UTF-8 specification (RFC 3629)
- Could allow injection attacks in downstream processing

**Test Evidence**:
- `test_utf8_overlong_2byte`: Accepts 0xC1 0x81 and decodes to 'A'
- `test_utf8_overlong_3byte_slash`: Accepts 0xE0 0x80 0xAF and decodes to '/'
- `test_utf8_null_codepoint_overlong`: Accepts 0xC0 0x80 (overlong null)

**Recommendation**: Add validation to `decode_utf8_sequence()` to reject:
- 2-byte sequences with codepoint < 0x80
- 3-byte sequences with codepoint < 0x800
- 4-byte sequences with codepoint < 0x10000

---

### 2. UTF-16 Surrogate Acceptance ⚠️ CRITICAL

**Severity**: High (Data Corruption Risk)

**Description**: The parser accepts UTF-16 surrogates (U+D800-U+DFFF) encoded in UTF-8, which are invalid in UTF-8.

**Attack Vector**:
```
UTF-16 high surrogate U+D800:  0xED 0xA0 0x80  ← ACCEPTED BY PARSER
UTF-16 low surrogate U+DFFF:   0xED 0xBF 0xBF  ← ACCEPTED BY PARSER
```

**Why This Matters**:
- Surrogates are not valid Unicode codepoints
- Can cause corruption in downstream UTF-8 processing
- Violates UTF-8 specification
- May cause crashes or security issues in libraries expecting valid Unicode

**Test Evidence**:
- `test_utf8_surrogate_high`: Accepts 0xED 0xA0 0x80 and decodes to U+D800

**Recommendation**: Add validation to reject codepoints in range U+D800-U+DFFF

---

### 3. Out-of-Range Unicode Codepoint Acceptance ⚠️ MEDIUM

**Severity**: Medium (Compatibility Risk)

**Description**: The parser accepts codepoints beyond the valid Unicode range (> U+10FFFF).

**Attack Vector**:
```
Codepoint U+110000:  0xF4 0x90 0x80 0x80  ← ACCEPTED BY PARSER
```

**Why This Matters**:
- Unicode specification only defines codepoints up to U+10FFFF
- May cause issues in downstream processing
- Could be used for denial-of-service or confusion attacks

**Test Evidence**:
- `test_utf8_codepoint_too_large`: Accepts 0xF4 0x90 0x80 0x80 (U+110000)

**Recommendation**: Add validation to reject codepoints > 0x10FFFF

---

## Edge Cases That Work Correctly ✅

The following attack vectors were tested and handled correctly:

### State Confusion
- **UTF-8 interrupted by ESC**: ✅ Correctly resets UTF-8 state (ESC fails continuation byte validation)
- **Escape interrupted by UTF-8**: ✅ Correctly resets escape state (UTF-8 lead byte fails '[' validation)

### Invalid UTF-8
- **Invalid lead bytes (0xF8-0xFF)**: ✅ Correctly rejected as UNKNOWN
- **Continuation byte without lead**: ✅ Correctly rejected as UNKNOWN
- **Invalid continuation bytes**: ✅ Correctly detected and state reset

### Escape Sequences
- **Buffer overflow protection**: ✅ Correctly resets at 15 bytes (buffer size - 1)
- **Invalid escape sequences**: ✅ Correctly reset and recover
- **Rapid state transitions**: ✅ Handles ESC/UTF-8 cycling correctly
- **Null bytes in escape sequences**: ✅ Handled gracefully (incomplete sequence)
- **Control chars in escape sequences**: ✅ Handled gracefully

---

## Recommendations

### Immediate Actions (CRITICAL)

1. **Add UTF-8 Validation to `decode_utf8_sequence()`**:
   ```c
   static uint32_t decode_utf8_sequence(const char *buf, size_t len)
   {
       // ... existing decoding logic ...

       // Reject overlong encodings
       if (len == 2 && codepoint < 0x80) return 0xFFFD; // Replacement char
       if (len == 3 && codepoint < 0x800) return 0xFFFD;
       if (len == 4 && codepoint < 0x10000) return 0xFFFD;

       // Reject surrogates
       if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return 0xFFFD;

       // Reject out-of-range
       if (codepoint > 0x10FFFF) return 0xFFFD;

       return codepoint;
   }
   ```

2. **Update Tests**: Add validation tests to ensure invalid sequences return replacement character or UNKNOWN

3. **Security Audit**: Review all downstream consumers of codepoints to ensure they handle U+FFFD correctly

### Future Enhancements

1. **Add UTF-8 Conformance Tests**: Include W3C/Unicode test vectors for UTF-8 validation
2. **Fuzz Testing**: Use AFL or libFuzzer to find additional edge cases
3. **Security Documentation**: Document expected UTF-8 behavior and security guarantees

---

## Test Coverage

Created `tests/integration/input_pathological_integration_test.c` with 14 pathological tests:

1. ✅ State confusion: UTF-8 then escape
2. ✅ State confusion: escape then UTF-8
3. ⚠️ Overlong 2-byte encoding (VULNERABILITY DETECTED)
4. ⚠️ Overlong 3-byte encoding (VULNERABILITY DETECTED)
5. ✅ Invalid lead byte 0xF8
6. ✅ Continuation without lead
7. ⚠️ UTF-16 surrogate (VULNERABILITY DETECTED)
8. ⚠️ Out-of-range codepoint (VULNERABILITY DETECTED)
9. ⚠️ Null codepoint overlong (VULNERABILITY DETECTED)
10. ✅ Escape with null byte
11. ✅ Escape with control char
12. ✅ Nearly full buffer
13. ✅ Rapid ESC transitions
14. ✅ Multiple incomplete UTF-8

---

## Impact Assessment

### Current Risk Level: **MEDIUM-HIGH**

**Attack Surface**:
- Direct terminal input from users
- Potential for injection attacks if downstream processing doesn't validate
- Directory traversal risk if file paths are constructed from input

**Mitigation Factors**:
- Parser doesn't crash (robustness is good)
- State management is sound (no persistent corruption)
- Buffer overflow protection works correctly

**Exploitation Difficulty**: Low-Medium
- Attack requires specific knowledge of UTF-8 encoding
- But overlong encoding attacks are well-documented

---

## Testing Methodology

1. **Hacker Mindset Approach**: Attempted to break parser through:
   - State confusion (interleaving sequences)
   - Malformed UTF-8 (overlong, surrogates, invalid ranges)
   - Escape sequence abuse (buffer overflow, invalid sequences)
   - Rapid state transitions

2. **Known Attack Patterns**: Tested historical UTF-8 vulnerabilities:
   - Overlong encodings (CVE-2000-0884, CVE-2008-2938)
   - Surrogate attacks
   - Range violations

3. **Comprehensive Coverage**: 14 tests covering 7 distinct vulnerability classes

---

## Conclusion

The input parser demonstrates **good robustness** in handling malformed input without crashing, and **excellent state management** for escape sequences. However, the UTF-8 decoder has **critical security vulnerabilities** that accept invalid encodings.

**Action Required**: Implement UTF-8 validation as recommended above to prevent potential security bypass attacks.

---

*Analysis conducted: 2025-11-08*
*Analyst: Claude (Hacker Mindset Security Review)*
*Test Suite: `tests/integration/input_pathological_integration_test.c`*
