FROM ubuntu:22.04
RUN apt update
RUN apt install git build-essential pkg-config vim curl python3 -y
RUN git clone https://github.com/pjsip/pjproject.git
WORKDIR pjproject
RUN git checkout 2.12.1
ENV CFLAGS="-fPIC"
RUN ./configure 
RUN make
RUN make dep
RUN make clean
RUN make 
RUN make install
WORKDIR /
RUN mkdir app
WORKDIR app
RUN curl https://nodejs.org/dist/v16.18.0/node-v16.18.0-linux-x64.tar.xz >node-v16.tar.gz
RUN tar -xvf node-v16.tar.gz
ENV PATH="/app/node-v16.18.0-linux-x64/bin:${PATH}"
RUN mkdir node_modules
COPY pjsua-wrapper node_modules/pjsua-wrapper/
WORKDIR /app/node_modules/pjsua-wrapper
RUN npm install
WORKDIR /

