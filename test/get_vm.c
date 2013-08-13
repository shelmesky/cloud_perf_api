    /*
    //获取所有XenServer
    xen_host_set *hosts = xen_host_set_alloc(20);
    xen_host_get_all(session, &hosts);
    
    int hosts_size = hosts->size, i;
    //查看每个XenServer
    for(i=0; i<hosts_size; i++) {
        xen_host_record *host_record = xen_host_record_alloc();
        xen_host_get_record(session, &host_record, hosts->contents[i]);
        fprintf(stderr, "XenServer: \n%s\n%s\n%s\n%s\n\n",host_record->uuid, host_record->name_label,
                        host_record->name_description, host_record->address);
        xen_host_record_free(host_record);
        
        //获取所有虚拟机
        xen_vm_set * vms = xen_vm_set_alloc(100);
        xen_vm_get_all(session, &vms);
        
        fprintf(stderr, "All vm: %d\n", (int)vms->size);
        
        int vm_size = vms->size, j;
        //查看每台虚拟机
        //并排除模板、快照、和管理域的虚拟机
        for(j=0; j<vm_size; j++) {
            xen_vm_record *vm_record = xen_vm_record_alloc();
            xen_vm_get_record(session, &vm_record, vms->contents[j]);
            
            if(!vm_record->is_a_snapshot && !vm_record->is_a_template &&
                !vm_record->is_control_domain && !vm_record->is_snapshot_from_vmpp)
            {
                fprintf(stderr, "%s\n%s\n\n",
                        vm_record->uuid,
                        vm_record->name_label);
                
            //获取主机的所有网卡
            xen_vif_set *ifs = xen_vif_set_alloc(10);
            xen_vm_get_vifs(session, &ifs, vms->contents[j]);
            
            int vif_size = ifs->size, k;
            //查看每个网卡
            for(k=0; k<vif_size; k++) {
                xen_vif_record *vif_record = xen_vif_record_alloc();
                xen_vif_get_record(session, &vif_record, ifs->contents[k]);
                fprintf(stderr, "nic %s:\nread: %d\nwrite: %d\n",
                        vif_record->device,
                        vif_record->metrics->u.record->io_read_kbs,
                        vif_record->metrics->u.record->io_write_kbs);
            }
            
            //获取主机所有的磁盘
            //xen_vm_get_vbds();
            }
        }
    }
    */