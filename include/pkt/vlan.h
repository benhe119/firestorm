#ifndef _PKT_VLAN_HEADER_INCLUDED_
#define _PKT_VLAN_HEADER_INCLUDED_

#define VLAN_ID 0xffffff
struct pkt_vlanhdr {
	uint16_t	vlan;
	uint16_t	proto;
} _packed;

#endif /* _PKT_VLAN_HEADER_INCLUDED_ */
