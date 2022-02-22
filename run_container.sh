
function run_container()  {
  image_name="dealii/dealii:latest"
  xhost +local:root
  XSOCK=/tmp/.X11-unix
  docker run -it --rm \
     -e DISPLAY=$DISPLAY \
     --name microbubble \
     --privileged \
     -v $(pwd)/:/home/dealii/src \
     -v $XSOCK:$XSOCK \
     -v $HOME/.ssh:/home/dealii/.ssh \
     -v $HOME/.Xauthority:/home/dealii/.Xauthority \
     $image_name "$@"
}

run_container
