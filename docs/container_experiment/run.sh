sudo docker stop -t 0 `sudo docker ps -q` &> /dev/null
sudo docker rm `sudo docker ps -a -q` &> /dev/null
sudo docker run --name mcd -d --rm memcd
SERVER_IP=`sudo docker inspect mcd -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'`
echo $SERVER_IP
sudo docker run -it --name mutilate --rm --env SERVER_IP=$SERVER_IP mutilate /bin/bash
