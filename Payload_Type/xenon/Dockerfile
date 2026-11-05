FROM itsafeaturemythic/mythic_python_base:latest

# Install C build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    mingw-w64 \
    nasm \
    make \
    openjdk-17-jdk

RUN python3 -m pip install toml donut-shellcode

# Mythic v4 requires container protocol v1.5.x; the base image ships mythic_container 0.6.x (v1.4.3)
RUN python3 -m pip install --upgrade "mythic_container==0.7.0rc10"

WORKDIR /Mythic/

COPY [".", "."]

CMD ["python3", "main.py"]
