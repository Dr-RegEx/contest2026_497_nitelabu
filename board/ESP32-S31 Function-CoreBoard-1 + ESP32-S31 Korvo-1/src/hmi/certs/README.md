# Product TLS trust

`ISRG_Root_X1.pem` is a public CA certificate, not a device key. The CMake HMI
ROMFS rule installs it as `/etc/ssl/curl/ca-certificates.crt` when curl is enabled.
curl retains certificate and hostname verification. No insecure fallback is set.

Source: https://letsencrypt.org/certs/isrgrootx1.pem

Certificate catalog: https://letsencrypt.org/certificates/

DER SHA-256:
`96bcec06264976f37460779acf28c5a7cfe8a3c0aae11a8ffcee05c0bddf08c6`

This deliberately limited trust set supports the selected X1 test endpoints.
A product must choose and maintain the roots required by its own servers and
plan signed trust-store/firmware updates. It is not a general browser CA bundle.
Validation also needs a usable hardware RNG and a correct system clock.
The current explicit NTP command uses unauthenticated NTP; authenticated time,
certificate revocation and device identity provisioning are separate work.
