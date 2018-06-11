FROM docker-enterprise.barco.com/eis/baseos9-minimal:1.1.3
ARG PACKAGE_VERSION
ARG DISTRIBUTION
COPY results/*.deb /packages/
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    /packages/*.deb && \
    apt-get autoremove -y && \
    apt-get autoclean -y && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-x \
    git adduser coreutils ca-certificates curl sudo && \
    apt-get autoremove -y && \
    apt-get autoclean -y && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
RUN git clone https://github.com/sstephenson/bats.git && cd bats && ./install.sh /usr/local && cd - && rm -rf bats
COPY jenkins-test/ /test
CMD ["/test/testsuites/entrypoint.sh"]
