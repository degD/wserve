
# WSERVE: Educational Web Server in Pure C

## uHTTP/1.1 (WIP)

**uHTTP (micro-HTTP)** is a custom HTTP/1.1 subset protocol. `wserve` supports uHTTP.

- Only supports GET, HEAD, POST methods.
- Only IPv4.
- Short-lived connections.
- Only supports `Content-Length` header for body size.
- Supports a large MIME set.
- Requests with unsupported features will get rejected.
- Rejections of unsupported features return `418 I'm a teapot`.
- Return `404` if requested file does not exist.
- Return `405` if requested route does exist.

## MIME Types

- application/octet-stream
- audio/aac
- application/x-abiword
- image/apng
- application/x-freearc
- image/avif
- video/x-msvideo
- application/vnd.amazon.ebook
- application/octet-stream
- image/bmp
- application/x-bzip
- application/x-bzip2
- application/x-cdf
- application/x-csh
- text/css
- text/csv
- application/msword
- application/vnd.openxmlformats-officedocument.wordprocessingml.document
- application/vnd.ms-fontobject
- application/epub+zip
- application/gzip
- image/gif
- text/html
- text/html
- image/vnd.microsoft.icon
- text/calendar
- application/java-archive
- image/jpeg
- image/jpeg
- text/javascript
- application/json
- application/ld+json
- text/markdown
- audio/midi
- audio/midi
- text/javascript
- audio/mpeg
- video/mp4
- video/mpeg
- application/vnd.apple.installer+xml
- application/vnd.oasis.opendocument.presentation
- application/vnd.oasis.opendocument.spreadsheet
- application/vnd.oasis.opendocument.text
- audio/ogg
- video/ogg
- application/ogg
- audio/ogg
- font/otf
- application/pdf
- application/x-httpd-php
- image/png
- application/vnd.ms-powerpoint
- application/vnd.openxmlformats-officedocument.presentationml.presentation
- application/vnd.rar
- application/rtf
- application/x-sh
- image/svg+xml
- application/x-tar
- image/tiff
- image/tiff
- video/mp2t
- font/ttf
- text/plain
- application/vnd.visio
- audio/wav
- audio/webm
- video/webm
- application/manifest+json
- image/webp
- font/woff
- font/woff2
- application/xhtml+xml
- application/vnd.ms-excel
- application/vnd.openxmlformats-officedocument.spreadsheetml.sheet
- application/xml
- application/vnd.mozilla.xul+xml
- application/zip
- video/3gpp
- video/3gpp2
- application/x-7z-compressed
