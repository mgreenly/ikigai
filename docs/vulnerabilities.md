# Security Vulnerabilities

CVE vulnerability status for dependencies built from source and statically linked.

**Last updated**: 2025-11-07

**Note**: Distro-provided packages (Debian/Fedora) receive security patches from their respective maintainers. This document focuses on libraries we build from source.

## Summary

| Library | Built from Source | 2024-2025 CVEs | Status |
|---------|-------------------|----------------|--------|
| ulfius | Fedora only | 0 | ✅ Clean |
| yder | Fedora only | 0 | ✅ Clean |
| orcania | Fedora only | 0 | ✅ Clean |
| libb64 | No (unmaintained) | 0 | ⚠️ Monitor |

## Detailed Findings

### ulfius (Built from Source - Fedora)

**Status**: No vulnerabilities for 2024-2025

**Version used**: 2.7.15

**Historical CVEs**:
- CVE-2021-40540 (fixed in version 2.7.4, September 2021)

**Repository**: https://github.com/babelouest/ulfius

### yder (Built from Source - Fedora)

**Status**: No vulnerabilities for 2024-2025

**Version used**: 1.4.20

**Repository**: https://github.com/babelouest/yder

### orcania (Built from Source - Fedora)

**Status**: No vulnerabilities for 2024-2025

**Version used**: 2.3.3

**Repository**: https://github.com/babelouest/orcania

### libb64 (Distro Package - Unmaintained)

**Status**: No vulnerabilities found for 2024-2025

**Notes**:
- Library is old (last updated ~2010)
- Not actively maintained
- No known CVEs in recent years
- Consider alternatives if security becomes a concern

## Action Items

1. **Monitor from-source dependencies**: Check for CVEs quarterly
   - ulfius 2.7.15
   - yder 1.4.20
   - orcania 2.3.3
2. **libb64**: Consider alternatives if security becomes critical (unmaintained)

**Note**: All other dependencies use distro packages that receive security patches from Debian/Fedora maintainers.

## Update Process

When updating dependencies:

1. Update version in relevant Dockerfile(s)
2. Run `make distro-check` to validate across all distributions
3. Run full CI suite: `make ci`
4. Update this document with new versions and CVE status

## References

- CVE Details: https://www.cvedetails.com/
- National Vulnerability Database: https://nvd.nist.gov/
- ulfius releases: https://github.com/babelouest/ulfius/releases
- yder releases: https://github.com/babelouest/yder/releases
- orcania releases: https://github.com/babelouest/orcania/releases
