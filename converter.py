#!/usr/bin/python
import parse_rrd
import time


def print_latest_vm_data(rrd_updates, uuid):
    print "**********************************************************"
    print "Got values for VM: "+uuid
    print "**********************************************************"
    for param in rrd_updates.get_vm_param_list(uuid):
        if param != "":
            max_time=0
            data=""
            for row in range(rrd_updates.get_nrows()):
                epoch = rrd_updates.get_row_time(row)
                dv = str(rrd_updates.get_vm_data(uuid,param,row))
                if epoch > max_time:
                    max_time = epoch
                    data = dv
        nt = time.strftime("%H:%M:%S", time.localtime(max_time))
        print "%-30s             (%s , %s)" % (param, nt, data)


# This function receive string arg
# and return string
def converter(arg):
	obj = parse_rrd.RRDUpdates()
	obj.load(arg)
	for uuid in obj.get_vm_list():
		print_latest_vm_data(obj, uuid)
