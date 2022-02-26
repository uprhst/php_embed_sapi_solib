# php_embed_sapi_solib
Embeddable PHP SAPI with threaded support (thread per request)

Inspired by: https://github.com/basvanbeek/embed2-sapi

This is mostly for educational purposes, hacking into PHP internals, seeing how things work. This was embedded into an event based Rust HTTP/S webserver powered by Mio, as a PoC of working with embed PHP instead of CGI/FPM. Still incomplete and complete unoptimized. Most logical way of handling PHP is using php-fpm, it is more optimized and can easily scaled.

Not abandoned, but will not come back to it for awhile.
