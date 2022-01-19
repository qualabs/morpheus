
## Morpheus

Morhpeus is an NGINX module that processes MPDs. It runs validation checks and can change MPD elements as part of packaging processing done by NGINX serving as a content origin. It can also run as a stand-alone program without NGINX as described below.

The goal of Morpheus is to adjust MPDs on the fly to conform to the DASH specification, correct errors, and test experimental features. Once built and installed, Morpheus runs on every file ending in "mpd" (\*.mpd) that is uploaded to the NGINX server using HTTP POST or PUT by the media presentation author.

### Building

Morpheus compiles with g++ 9.3.0 on Ubuntu 20.04 with NGINX 1.18.0. No other compilers or versions have been tried. Pugixml is a dependency and is used by Morpheus for all xml processing. The pugixml files that are required are included in this repo.

Compiling Morpheus as a dynamic library follows the standard method for NGINX modules outlined [here](https://www.nginx.com/blog/compiling-dynamic-modules-nginx-plus/). One thing to note is since Morpheus uses c++, the `configure` command should include the standard c++ lib, like so:

```
./configure --with-ld-opt="-lstdc++" --with-compat --add-dynamic-module=../nginx-morpheus-module
```

### Installation

Once the dynamic lib ngx_http_morpheus_module.so has been created, it should be copied to the loadable lib dir of NGINX. In version 1.18.0 this is usually `/usr/lib/nginx/modules/` but is configurable in the NGINX conf file.

To enable the dash+xml application type, copy `mime_types.types` to `/etc/nginx/`.

The `nginx_morpheus.conf` in this repo is an example of a working config with Morpheus enabled. The normal steps on Ubuntu 20.04 to install the conf, mime.types, and module and restart NGINX with Morpheus are:

```
sudo systemctl stop nginx
sudo cp nginx/objs/ngx_http_morpheus_module.so /usr/lib/nginx/modules/
sudo cp nginx_morpheus.conf /etc/nginx/nginx.conf
sudo cp mime.types /etc/nginx/
sudo systemctl start nginx
```

### Stand-alone

Morpheus can also be built as a stand-alone binary. The provided `Makefile` accomplishes this. For example:

```
make
```

will build an executable called `morphdriver` which can then be run locally on MPD files. `morphdriver` does the same processing as the Morpheus NGINX plugin and adds a few additional features. Given a CKM response for an encryption context in xml format, it adds ContentProtection elements to the AdaptationSets in the mpd. It can also add an I frame track for trick modes if an I frames mpd is provided.

`morphdriver` without arguments or `morphdriver -h` shows the usage


```
$ ./morphdriver -h
morpheus VOD stand-alone tool
Usage:
  ./morphdriver [OPTION...]

  -n arg      encoder mpd file
  -i arg      iframes track mpd file
  -d arg      ckm encrypt context response xml file
  -h, --help  Print help
```

Building the `morphdriver` executable requires cxxopts.hpp, which is also included. 


