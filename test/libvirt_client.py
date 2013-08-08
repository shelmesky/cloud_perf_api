#!/usr/bin/python
import sys

try:
    import libvirt as _libvirt
except (ImportError,ImportWarning) as e:
    print "Can not find python-libvirt, in ubuntu just run \"sudo apt-get install python-libvirt\"."
    print e
    sys.exit(1)


def list_uuids(host):
    dom_ids = []
    uri = 'xen+tcp://%s/system' % host
    try:
        conn = _libvirt.open(uri)
    except Exception,e:
        print 'libvirt error: can not connect to remote libvirtd'
        raise e
    domain_ids = conn.listDomainsID()
    for domain_id in domain_ids:
        dom = conn.lookupByID(domain_id)
        dom_ids.append(dom.UUIDString())
    print dom_ids

list_uuids('192.2.4.63')
