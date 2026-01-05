FROM itsafeaturemythic/mythic_python_base:latest

# Install C build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    mingw-w64 \
    nasm \
    make \
    openjdk-11-jdk

RUN python3 -m pip install toml donut-shellcode

WORKDIR /Mythic/

CMD ["python3", "main.py"]
