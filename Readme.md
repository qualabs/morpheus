
## Morpheus

Morhpeus is an NGINX module that processes MPDs. It runs validation checks and can change MPD elements as part of packaging processing done by NGINX serving as a content origin.

The goal of Morpheus is to adjust MPDs on the fly to conform to the DASH specification, correct errors, and test experimental features. Once built and installed, Morpheus runs on every file ending in suffix ``mpd'' (\*.mpd) that is uploaded to the NGINX server using HTTP POST or PUT by the media presentation author.

### Building

Morpheus compiles with g++ 9.3.0 on Ubuntu 20.04 with NGINX 1.18.0. No other compilers or versions have been tried. Pugixml is a dependency and is used by Morpheus for all xml processing. The pugixml files that are required are included in this repo.

Compiling Morpheus as a dynamic library follows the standard method for NGINX modules outlined [here](https://www.nginx.com/blog/compiling-dynamic-modules-nginx-plus/). One thing to note is since Morpheus uses c++, the `configure` command should include the standard c++ lib, like so:

```
./configure --with-ld-opt="-lstdc++" --with-compat --add-dynamic-module=../nginx-morpheus-module
```

### Installation

Once the dynamic lib ngx_http_morpheus_module.so has been created, it should be copied to the loadable lib dir of NGINX. In version 1.18.0 this is usually `/usr/lib/nginx/modules/` but is configurable in the NGINX conf file.

The `nginx_morpheus.conf` file in this repo is an example of a working config with Morpheus enabled. The normal steps on Ubuntu 20.04
to install this conf file and restart NGINX with Morpheus are:

```
sudo systemctl stop nginx
sudo cp nginx_morpheus.conf /etc/nginx/nginx.conf
sudo systemctl start nginx
```



