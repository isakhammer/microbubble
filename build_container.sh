
function build_container()  {
  image_name="dealii_micro:latest"
  docker build . -t $image_name
}

build_container
