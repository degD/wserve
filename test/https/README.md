
# Certificates and How to Generate Them

The certificates inside this file are self-signed by me. Do not use them in any kind of production, ever. You have been warned.

The following command is used to generate keys:

```
openssl req -x509 -newkey rsa:2048 \
  -keyout test/https/server.key \
  -out test/https/server.crt \
  -days 365 \
  -nodes \
  -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
```

The following command is used to convert files to DER format, as `wserve` only supports raw DER format:

```
# Certificate: PEM -> DER
openssl x509 -in test/https/server.crt \
  -outform DER -out test/https/server-cert.der

# RSA private key: PEM -> DER (PKCS#1)
openssl pkey -in test/https/server.key \
  -outform DER -out test/https/server-key.der
```

The server tested with a single self-signed certificate, but not tested with a genuine CA-issued certificate chain.
