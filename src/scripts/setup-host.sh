#!/usr/bin/env bash

dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

REMOTE_USERNAME=ubuntu

host="$1"
key="$2"

ssh_cmd="ssh -i ${key}"

echo "Copying stuff to host ${host}"

$ssh_cmd "$host" -t "mkdir /home/${REMOTE_USERNAME}/cxlbuf/"

rsync --exclude='*.o' --exclude='*.a' --exclude='*.so' -avP -e "${ssh_cmd}" "${dir}/../../" "${host}:/home/${REMOTE_USERNAME}/cxlbuf/"

$ssh_cmd "$host" -t "/home/${REMOTE_USERNAME}/cxlbuf/src/scripts/deps.sh"
