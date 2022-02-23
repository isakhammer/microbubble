
function build_container()  {
  image_name="custom_dealii_v2:latest"
  docker build . -t $image_name
}

build_container
