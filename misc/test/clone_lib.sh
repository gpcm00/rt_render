case $1 in
    stb)
        git clone git@github.com:nothings/stb.git
        ;;
    tinyobjloader)
        git clone https://github.com/tinyobjloader/tinyobjloader
        ;;
    glTF-Assets)
        git clone git@github.com:KhronosGroup/glTF-Sample-Assets.git
        ;;
    *)
        echo "Read the source file to check how to use it"
        ;;
esac