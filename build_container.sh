
function build_container()  {
  image_name="custom_dealii:latest"
  docker build . -t $image_name
}

build_container
