FROM ghcr.io/phusion/holy-build-box/hbb-64

VOLUME ["/io"]
WORKDIR /io

RUN yum install clang SDL2-devel -y && \
    yum clean all && \
    rm -rf /var/cache/yum

CMD ./build.sh
