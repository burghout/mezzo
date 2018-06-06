set -e 

for file in ./unittests/bin/*
do
    ${file}
done
