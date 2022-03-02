
function run_container2()  {
  # image_name="custom_dealii:latest"
  # image_name="dealii/dealii:master-focal-root"
  image_name="dealii_micro:latest"
  xhost +local:root
  XSOCK=/tmp/.X11-unix
  docker run -it --rm \
     -e DISPLAY=$DISPLAY \
     --name dealii_micro \
     --privileged \
     -v $(pwd)/:/root/code \
     -v $XSOCK:$XSOCK \
     -v $HOME/.ssh:/root/.ssh \
     -v $HOME/.Xauthority:/root/.Xauthority \
     $image_name "$@"
}


run_container2
