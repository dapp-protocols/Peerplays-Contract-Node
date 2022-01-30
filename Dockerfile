FROM containers.followmyvote.com:443/pollaris/peerplays-dapp-support/master
LABEL maintainer="Nathaniel Hourt <nathan@followmyvote.com>"

ADD . /src/PeerplaysNode
RUN /bin/bash -ec "apk update; \
    apk add capnproto-dev; \
    mkdir /src/build; \
    cd /src/build; \
    cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja ../PeerplaysNode; \
    ninja; \
    adduser -h /Node -D node; \
    cp PeerplaysNode /Node; \
    cd /Node; \
    mkdir -p '/Node/.config/Follow My Vote/PollarisBackend'; \
    chown node:node -R /Node; \
    rm -rf /src"

USER node
WORKDIR /Node
VOLUME ["/Node/.config/Follow My Vote/PollarisBackend"]
EXPOSE 2776
ENTRYPOINT "/Node/PeerplaysNode"
