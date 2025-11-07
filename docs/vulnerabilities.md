# Security Vulnerabilities

CVE vulnerability status for dependencies used in ikigai.

**Last updated**: 2025-11-07

## Summary

| Library | 2024-2025 CVEs | Status | Action Required |
|---------|----------------|--------|-----------------|
| libtalloc | 0 | ✅ Clean | None |
| libjansson | 0 | ✅ Clean | None |
| libuuid (util-linux) | 1 | ⚠️ Low risk | Monitor |
| libb64 | 0 | ✅ Clean | None (unmaintained) |
| libulfius | 0 | ✅ Clean | None |
| libcurl | 6 | ⚠️ Action needed | Update to 8.12.0+ |

## Detailed Findings

### libtalloc

**Status**: No vulnerabilities found for 2024-2025

- Part of Samba project
- CVEs exist for Samba's usage of talloc, not the library itself
- Most recent related: CVE-2023-34967 (Samba vulnerability involving talloc functions)

### libjansson

**Status**: No upstream vulnerabilities for 2024-2025

- CVE-2024-39349 affects only Synology camera firmware (vendor-specific, not upstream)
- Historical CVEs (pre-2024):
  - CVE-2016-4425 - Stack exhaustion/deep recursion
  - CVE-2013-6401 - Hash collision DoS

### libuuid (util-linux)

**Status**: One vulnerability, low risk for our usage

#### CVE-2024-28085 (WallEscape)
- **CVSS**: 8.4 (High)
- **Affected versions**: util-linux ≤ 2.40
- **Component**: `wall` command (not libuuid)
- **Description**: The wall command allows escape sequences to be sent to other users' terminals through argv
- **Impact on ikigai**: None - we only use libuuid, not the wall command
- **Status**: Monitor

### libb64

**Status**: No vulnerabilities found for 2024-2025

**Notes**:
- Library is old (last updated ~2010)
- Not actively maintained
- No known CVEs in recent years
- Consider alternatives if security becomes a concern

### libulfius

**Status**: No vulnerabilities for 2024-2025

- Only CVE: CVE-2021-40540 (fixed in version 2.7.4, September 2021)
- We use version 2.7.15 (built from source in Fedora Dockerfile)

### libcurl

**Status**: Multiple vulnerabilities - update recommended

#### 2024 Vulnerabilities

**CVE-2024-6197** - Use-after-free
- **CVSS**: Not specified
- **Affected versions**: 8.6.0 - 8.8.0
- **Description**: Use-after-free vulnerability in curl and libcurl
- **Introduced**: January 31, 2024 (version 8.6.0)

**CVE-2024-7264** - ASN.1 date parser overread
- **CVSS**: Not specified
- **Affected**: Only when built with GnuTLS, Schannel, Secure Transport, or mbedTLS
- **Description**: Parser overread in ASN.1 date handling

**CVE-2024-0853** - Security restrictions bypass
- **CVSS**: Not specified
- **Description**: Security restrictions bypass vulnerability

#### 2025 Vulnerabilities

**CVE-2025-0725** - Integer overflow to buffer overflow
- **CVSS**: Not specified
- **Description**: Integer overflow leading to buffer overflow

**CVE-2025-0167** - Confidentiality, integrity, availability impact
- **CVSS**: Not specified
- **Description**: Impacts CIA triad

**CVE-2025-0665** - Confidentiality, integrity, availability impact
- **CVSS**: Not specified
- **Description**: Impacts CIA triad

**Recommendation**: Update to curl/libcurl 8.12.0 or later

## Action Items

1. **libcurl**: Update Dockerfiles to use curl/libcurl 8.12.0+
2. **libb64**: Monitor for updates; consider alternatives if maintenance becomes critical
3. **All libraries**: Regular CVE monitoring (quarterly recommended)

## Update Process

When updating dependencies:

1. Update version in relevant Dockerfile(s)
2. Run `make distro-check` to validate across all distributions
3. Run full CI suite: `make ci`
4. Update this document with new versions and CVE status

## References

- CVE Details: https://www.cvedetails.com/
- National Vulnerability Database: https://nvd.nist.gov/
- curl security: https://curl.se/docs/security.html
- Samba security: https://www.samba.org/samba/security/
