xmake
xmake r > output.txt
# delta --side-by-side output.txt master-output.txt
# delta --side-by-side <(head -n 20 output.txt) <(head -n 20 master-output.txt)
