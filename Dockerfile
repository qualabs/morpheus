# Stage 1: Build the dynamic module
FROM nginx:1.24.0 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    libpcre3-dev \
    zlib1g-dev \
    libssl-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Download nginx source matching the base image version
RUN wget http://nginx.org/download/nginx-1.24.0.tar.gz \
    && tar -xzf nginx-1.24.0.tar.gz \
    && rm nginx-1.24.0.tar.gz

# Remove -Werror from nginx build system — it breaks C++ compilation
RUN sed -i 's/ -Werror//' /nginx-1.24.0/auto/cc/gcc

COPY . /morpheus/

# Download pugixml v1.15 source from the official repository.
# Runs after COPY so it works regardless of whether local pugixml/ files are present.
RUN mkdir -p /morpheus/pugixml \
    && wget -qO /morpheus/pugixml/pugixml.cpp  https://raw.githubusercontent.com/zeux/pugixml/v1.15/src/pugixml.cpp \
    && wget -qO /morpheus/pugixml/pugixml.hpp  https://raw.githubusercontent.com/zeux/pugixml/v1.15/src/pugixml.hpp \
    && wget -qO /morpheus/pugixml/pugiconfig.hpp https://raw.githubusercontent.com/zeux/pugixml/v1.15/src/pugiconfig.hpp

WORKDIR /nginx-1.24.0
RUN ./configure \
        --with-ld-opt="-lstdc++" \
        --with-cc-opt="-Wno-write-strings" \
        --with-compat \
        --add-dynamic-module=/morpheus \
    && make modules


# Stage 2: Production image
FROM nginx:1.24.0

# Copy the compiled module
COPY --from=builder /nginx-1.24.0/objs/ngx_http_morpheus_module.so /etc/nginx/modules/

# Copy nginx configuration and MIME types
COPY nginx_morpheus.conf /etc/nginx/nginx.conf
COPY mime.types /etc/nginx/mime.types

# nginx_morpheus.conf includes /usr/share/nginx/modules/*.conf which doesn't
# exist in the Docker image (unlike Ubuntu package installs); create it empty
RUN mkdir -p /usr/share/nginx/modules/

EXPOSE 80

# /dev/shm is a tmpfs mounted at container runtime, so its subdirectories
# cannot be created at build time — create them before starting nginx
CMD ["/bin/sh", "-c", "mkdir -p /dev/shm/nginx/client_temp && exec nginx -g 'daemon off;'"]
