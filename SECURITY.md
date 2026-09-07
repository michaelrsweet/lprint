Security Policy
===============

This file describes how security issues are reported and handled, and what the
expectations are for security issues reported to this project.


Supported Versions
------------------

This security policy only applies to production releases of this software.  A
production release is tagged and given a semantic version number of the form
"MAJOR.MINOR.PATCH" where "MAJOR" is an integer starting at 1 and "MINOR" and
"PATCH" are integers starting at 0.

> *Note:* Please report security vulnerabilities that only affect unreleased
> code as regular GitHub issues to
> <https://github.com/michaelrsweet/lprint/issues>.


What is a Security Bug?
-----------------------

Not every bug is a security bug.

The following kinds of issues are generally treated as security
vulnerabilities:

- Daemon/service crashes/hangs caused by a network request,
- Remote code execution through an API or other interface used by PAPPL,
- Privilege escalation that allows unauthorized actions or information
  disclosure, and
- Common weaknesses (buffer overflow, divide-by-zero, input validation,
  use-after-free, etc.) that lead to a demonstrated (not theoretical) exploit.

The following kinds of issues are generally treated as regular bugs:

- Vulnerabilities caused by mis-configuration,
- Issues caused by incorrect API usage, and
- Issues that only exist in non-production software.

Regular bugs should be reported to the project using the GitHub (public) issue
tracker page at <https://github.com/michaelrsweet/lprint/issues>.


Reporting a Security Bug
------------------------

Vulnerabilities should be reported to the project security advisory page at
<https://github.com/michaelrsweet/lprint/security/advisories>.

Provide details, impact, reproducer, affected versions, workarounds, and a
patch for the vulnerability, if applicable.

You can expect a response within 5 business days.


Security Bug Processing
-----------------------

Security bugs are processed privately until disclosed.

A Common Vulnerability Scoring System (CVSS) score is calculated to determine
the severity of the issue.  Issues with a CVSS score of 7 or more use
*responsible disclosure* where the security issue (and its fix) is disclosed
only after a mutually-agreed period of time (the "embargo date").  The issue
and fix are shared amongst and reviewed by the key stakeholders and the
CERT/CC, and a CVE is requested from the GitHub CNA.  Fixes are released to the
public on the agreed-upon date.

Fixes for less serious issues are pushed to the public repository and published
as soon as they are ready using the GitHub Security Advisory (GHSA) identifier.

Security bug changes are listed in the `CHANGES.md` file with a SECURITY prefix
and the CVSS score, for example:

- SECURITY-7.8: Description of serious issue (CVE-YYYY-NNNNN)
- SECURITY-2.2: Description of less serious issue (GHSA-xxxx-xxxx-xxx)
