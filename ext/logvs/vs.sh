#/bin/sh
rm -rf /tmp/*.txt
rm -rf /tmp/*.log
echo ======================================================================
# for i in {1..5}; do /usr/bin/time -v ./logvs none $*; done
# echo ----------------------------------------------------------------------
# for i in {1..5}; do /usr/bin/time -v ./logvs spdlog $*; done
# echo ----------------------------------------------------------------------
for i in {1..5}; do /usr/bin/time -v ./logvs mal $*; done
echo ----------------------------------------------------------------------
for i in {1..5}; do /usr/bin/time -v ./logvs fon9a $*; done
echo ----------------------------------------------------------------------
for i in {1..5}; do /usr/bin/time -v ./logvs fon9lf $*; done
echo ----------------------------------------------------------------------
for i in {1..5}; do /usr/bin/time -v ./logvs fon9 $*; done
echo ======================================================================
#ls -l /tmp
#tail /tmp/*txt /tmp/*log
wc -l /tmp/*txt /tmp/*log
rm -rf /tmp/*.txt
rm -rf /tmp/*.log
