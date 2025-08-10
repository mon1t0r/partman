#include "gpt.h"

void gpt_init(struct gpt_hdr *hdr)
{
    hdr->rev = gpt_hdr_rev;
    hdr->hdr_sz = gpt_hdr_sz;
    guid_create(&hdr->disk_guid);
    hdr->part_entry_sz = gpt_part_ent_sz;
}
