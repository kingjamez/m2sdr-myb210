#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>

 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/gfp.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <linux/device.h>
#include <linux/time.h>
#include <linux/pps_kernel.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>


#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
  

 
#define az_VENDOR_ID 0x10EE
#define az_DEVICE_ID 0x7012

#define DRV_NAME		"m2sdr"
#define PFX			DRV_NAME ": "

#ifndef PCI_EXP_DEVCTL_READRQ_4096B
#define PCI_EXP_DEVCTL_READRQ_4096B 0x5000

#endif

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("TQTT Liwei");
MODULE_DESCRIPTION("SDR WITH PCIE");




static struct pci_device_id az_ids[] = {
    {PCI_DEVICE(az_VENDOR_ID, az_DEVICE_ID)},
    {PCI_DEVICE(az_VENDOR_ID, 0x7021)},  //1x model
    {PCI_DEVICE(az_VENDOR_ID, 0x7022)},//2x model
{PCI_DEVICE(az_VENDOR_ID, 0x7024)},//4x model
    { }
};
static struct cdev my_device;


struct az_dmabuf_nfo {
    void* drv_addr          ; 
    dma_addr_t   phys       ;
    int len    ;
    u8 coherent; /* 1 = dma_alloc_coherent, 0 = dma_alloc_noncoherent */
};


#define C2H_MAX_BUF  64
#define H2C_MAX_BUF  64

#define MAX_BUFS   (C2H_MAX_BUF*2+H2C_MAX_BUF*2)

struct az_dev *d;
struct az_dev  *az_dev_root = NULL ;
struct az_dev {
    struct az_dev *next;   /* next device in list */
    struct pci_dev *pdev;     /* pci device */
    void __iomem *bar0_addr,*bar1_addr;
    struct az_dmabuf_nfo dma_bufs[MAX_BUFS];  
};

static wait_queue_head_t waitqueue;

static unsigned int poll_ms = 2;
module_param(poll_ms, uint, 0644);
MODULE_PARM_DESC(poll_ms, "Max ms libpcie waits for an MSI before re-polling the completion count");
static atomic_t   poll_cond, rd_cond ;



#define BAR0_WR(a,o,v  )	 iowrite32(cpu_to_be32(v), (void __iomem *)((unsigned long)(a) + 4*(o)))
#define BAR0_RD(a,o)         be32_to_cpu(ioread32((void __iomem *)((unsigned long)(a) + 4*(o))))


unsigned int rd_bar0_reg(struct az_dev *p, unsigned int n) { return BAR0_RD(p->bar0_addr,  n);}
unsigned int wr_bar0_reg(struct az_dev *p, unsigned int n, unsigned int v){BAR0_WR(p->bar0_addr, n,  v);return  0 ;}


/*
LO:
bit30..0 : phy_addr  >> 3;
bit31    : is_last

HI:
b31..24 :  buf_idx
b23...0 :  actual_len to transfer
*/ 
MODULE_DEVICE_TABLE(pci, az_ids);

/* Pi 5: 2 GiB PCIe inbound window, 64 MiB CMA hole at 0x3b800000. */
#define M2SDR_DMA_WIN_END   0x80000000ULL
#define M2SDR_CMA_HOLE_LO   0x3b000000ULL
#define M2SDR_CMA_HOLE_HI   0x40000000ULL
#define M2SDR_HOLD_MAX      96
/* Carve DMA from System RAM at 1 GiB so we never touch default CMA (HDMI). */
#define M2SDR_POOL_SIZE     (16u << 20)
#define M2SDR_POOL_PHYS_LO  0x40000000ULL
#define M2SDR_POOL_PHYS_HI  0x78000000ULL

static void *m2sdr_cma_sink;
static dma_addr_t m2sdr_cma_sink_dma;
static size_t m2sdr_cma_sink_sz;
static void *m2sdr_hold_va[M2SDR_HOLD_MAX];
static dma_addr_t m2sdr_hold_dma[M2SDR_HOLD_MAX];
static size_t m2sdr_hold_bytes[M2SDR_HOLD_MAX];
static int m2sdr_nhold;

static void *m2sdr_pool_va;
static unsigned long m2sdr_pool_pfn;
static unsigned long m2sdr_pool_nr;
static dma_addr_t m2sdr_pool_dma;
static size_t m2sdr_pool_sz;
static size_t m2sdr_pool_off;
static int m2sdr_pool_mapped;
static struct device *m2sdr_pool_dev;

static bool m2sdr_is_pool_va(const void *va)
{
	return m2sdr_pool_va && va &&
	       (const u8 *)va >= (const u8 *)m2sdr_pool_va &&
	       (const u8 *)va < (const u8 *)m2sdr_pool_va + m2sdr_pool_sz;
}

static void m2sdr_free_one(struct device *dev, struct az_dmabuf_nfo *b)
{
	if (!b || !b->drv_addr || !b->len)
		return;
	/* Pool slices are released with the whole 1 GiB pool. */
	if (!m2sdr_is_pool_va(b->drv_addr)) {
		if (b->coherent)
			dma_free_coherent(dev, b->len * 8, b->drv_addr, b->phys << 3);
		else
			dma_free_noncoherent(dev, b->len * 8, b->drv_addr,
					     b->phys << 3, DMA_BIDIRECTIONAL);
	}
	b->drv_addr = NULL;
	b->phys = 0;
	b->len = 0;
	b->coherent = 0;
}

static void m2sdr_sync_all(struct az_dev *p, enum dma_data_direction dir)
{
	int i;

	if (!p)
		return;
	for (i = 0; i < MAX_BUFS; i++) {
		if (!p->dma_bufs[i].drv_addr || p->dma_bufs[i].coherent)
			continue;
		dma_sync_single_for_cpu(&p->pdev->dev, p->dma_bufs[i].phys << 3,
					p->dma_bufs[i].len * 8, dir);
	}
}

static phys_addr_t m2sdr_page_phys(void *va)
{
	struct page *pg;

	if (!va)
		return 0;
	if (is_vmalloc_addr(va)) {
		pg = vmalloc_to_page(va);
		return pg ? page_to_phys(pg) : 0;
	}
	return virt_to_phys(va);
}

static unsigned long m2sdr_page_pfn(void *va)
{
	struct page *pg;

	if (!va)
		return 0;
	if (is_vmalloc_addr(va)) {
		pg = vmalloc_to_page(va);
		return pg ? page_to_pfn(pg) : 0;
	}
	return page_to_pfn(virt_to_page(va));
}

static bool m2sdr_dma_in_window(dma_addr_t dma, size_t bytes)
{
	return dma < M2SDR_DMA_WIN_END && (dma + bytes) <= M2SDR_DMA_WIN_END;
}

static bool m2sdr_dma_in_cma_hole(dma_addr_t dma, size_t bytes)
{
	dma_addr_t end = dma + bytes;

	return (dma < M2SDR_CMA_HOLE_HI && end > M2SDR_CMA_HOLE_LO);
}

static void m2sdr_release_pool(void)
{
	if (!m2sdr_pool_va)
		return;
	if (m2sdr_pool_mapped && m2sdr_pool_dev)
		dma_unmap_single(m2sdr_pool_dev, m2sdr_pool_dma,
				 m2sdr_pool_sz, DMA_BIDIRECTIONAL);
	free_contig_range(m2sdr_pool_pfn, m2sdr_pool_nr);
	printk(KERN_INFO PFX "released 1GiB pool pfn=0x%lx nr=%lu\n",
	       m2sdr_pool_pfn, m2sdr_pool_nr);
	m2sdr_pool_va = NULL;
	m2sdr_pool_pfn = 0;
	m2sdr_pool_nr = 0;
	m2sdr_pool_dma = 0;
	m2sdr_pool_sz = 0;
	m2sdr_pool_off = 0;
	m2sdr_pool_mapped = 0;
	m2sdr_pool_dev = NULL;
}

static int m2sdr_ensure_pool(struct device *dev)
{
	unsigned long nr, pfn, step;
	u64 phys;
	void *va;
	dma_addr_t dma;
	int ret, mapped, attempts, s;
	size_t try_sz[] = { M2SDR_POOL_SIZE, 8u << 20 };

	if (m2sdr_pool_va)
		return 0;

	for (s = 0; s < ARRAY_SIZE(try_sz); s++) {
		nr = try_sz[s] >> PAGE_SHIFT;
		step = pageblock_nr_pages;
		if (!step)
			step = nr;
		attempts = 0;
		for (phys = M2SDR_POOL_PHYS_LO;
		     phys + try_sz[s] <= M2SDR_POOL_PHYS_HI;
		     phys += (u64)step << PAGE_SHIFT) {
			if (++attempts > 80)
				break;
			pfn = PHYS_PFN(phys);
			if (!pfn_valid(pfn) || !pfn_valid(pfn + nr - 1))
				continue;
			ret = alloc_contig_range(pfn, pfn + nr, MIGRATE_MOVABLE,
						 GFP_KERNEL);
			if (ret)
				continue;
			va = page_address(pfn_to_page(pfn));
			if (!va) {
				free_contig_range(pfn, nr);
				continue;
			}
			mapped = 0;
			dma = dma_map_single(dev, va, try_sz[s], DMA_BIDIRECTIONAL);
			if (dma_mapping_error(dev, dma))
				dma = (dma_addr_t)phys;
			else
				mapped = 1;
			if (!m2sdr_dma_in_window(dma, try_sz[s]) ||
			    m2sdr_dma_in_cma_hole(dma, try_sz[s])) {
				if (mapped)
					dma_unmap_single(dev, dma, try_sz[s],
							 DMA_BIDIRECTIONAL);
				free_contig_range(pfn, nr);
				continue;
			}
			m2sdr_pool_va = va;
			m2sdr_pool_pfn = pfn;
			m2sdr_pool_nr = nr;
			m2sdr_pool_dma = dma;
			m2sdr_pool_sz = try_sz[s];
			m2sdr_pool_off = 0;
			m2sdr_pool_mapped = mapped;
			m2sdr_pool_dev = dev;
			printk(KERN_INFO PFX "1GiB pool %zu bytes pfn=0x%lx dma=%pad va=%px %s\n",
			       m2sdr_pool_sz, m2sdr_pool_pfn, &m2sdr_pool_dma,
			       m2sdr_pool_va, mapped ? "mapped" : "identity");
			return 0;
		}
	}
	printk(KERN_WARNING PFX "1GiB pool alloc_contig_range failed\n");
	return -ENOMEM;
}

static void m2sdr_drain_cma(struct device *dev)
{
	size_t try_sz[] = { 64u << 20, 48u << 20, 32u << 20, 16u << 20 };
	int i;

	if (m2sdr_cma_sink)
		return;
	for (i = 0; i < ARRAY_SIZE(try_sz); i++) {
		m2sdr_cma_sink = dma_alloc_coherent(dev, try_sz[i],
						    &m2sdr_cma_sink_dma, GFP_KERNEL);
		if (m2sdr_cma_sink) {
			m2sdr_cma_sink_sz = try_sz[i];
			printk(KERN_INFO PFX "CMA sink %zu bytes dma=%pad\n",
			       m2sdr_cma_sink_sz, &m2sdr_cma_sink_dma);
			return;
		}
	}
	printk(KERN_WARNING PFX "CMA sink alloc failed\n");
}

static void m2sdr_release_holds(struct device *dev)
{
	int i;

	for (i = 0; i < m2sdr_nhold; i++) {
		if (m2sdr_hold_va[i])
			dma_free_coherent(dev, m2sdr_hold_bytes[i],
					  m2sdr_hold_va[i], m2sdr_hold_dma[i]);
		m2sdr_hold_va[i] = NULL;
	}
	m2sdr_nhold = 0;
	if (m2sdr_cma_sink) {
		dma_free_coherent(dev, m2sdr_cma_sink_sz, m2sdr_cma_sink,
				  m2sdr_cma_sink_dma);
		m2sdr_cma_sink = NULL;
		m2sdr_cma_sink_sz = 0;
	}
	m2sdr_release_pool();
}

static void *m2sdr_alloc_dma(struct device *dev, size_t bytes, dma_addr_t *dma_out,
			     u8 *coherent_out)
{
	void *va;
	dma_addr_t dma;
	int tries;
	size_t n;

	/* Prefer a contiguous 1–2 GiB pool so FPGA DMA never lands in the
	 * 64 MiB CMA hole at 0x3b800000 (and so we do not relocate CMA). */
	if (!m2sdr_ensure_pool(dev)) {
		n = ALIGN(bytes, PAGE_SIZE);
		if (m2sdr_pool_off + n <= m2sdr_pool_sz) {
			va = (u8 *)m2sdr_pool_va + m2sdr_pool_off;
			dma = m2sdr_pool_dma + m2sdr_pool_off;
			m2sdr_pool_off += n;
			*dma_out = dma;
			*coherent_out = 0;
			return va;
		}
		printk(KERN_WARNING PFX "1GiB pool exhausted off=%zu need=%zu\n",
		       m2sdr_pool_off, n);
	}

	/* Ordinary pages first. dma_alloc_coherent on this 16 GiB Pi 5
	 * always came from the 64 MiB CMA hole at 0x3b800000, which the
	 * FPGA can be told to write while userspace never sees samples. */
	va = dma_alloc_noncoherent(dev, bytes, &dma, DMA_BIDIRECTIONAL, GFP_KERNEL);
	if (va) {
		if (m2sdr_dma_in_window(dma, bytes) &&
		    !m2sdr_dma_in_cma_hole(dma, bytes)) {
			*dma_out = dma;
			*coherent_out = 0;
			return va;
		}
		printk(KERN_INFO PFX "noncoherent reject dma=%pad bytes=%zu\n",
		       &dma, bytes);
		dma_free_noncoherent(dev, bytes, va, dma, DMA_BIDIRECTIONAL);
	}

	m2sdr_drain_cma(dev);
	for (tries = 0; tries < 48; tries++) {
		va = dma_alloc_coherent(dev, bytes, &dma, GFP_KERNEL);
		if (!va)
			return NULL;
		if (m2sdr_dma_in_window(dma, bytes) &&
		    !m2sdr_dma_in_cma_hole(dma, bytes)) {
			*dma_out = dma;
			*coherent_out = 1;
			return va;
		}
		printk(KERN_INFO PFX "reject dma=%pad bytes=%zu try=%d\n",
		       &dma, bytes, tries);
		if (m2sdr_nhold < M2SDR_HOLD_MAX) {
			m2sdr_hold_va[m2sdr_nhold] = va;
			m2sdr_hold_dma[m2sdr_nhold] = dma;
			m2sdr_hold_bytes[m2sdr_nhold] = bytes;
			m2sdr_nhold++;
		} else {
			dma_free_coherent(dev, bytes, va, dma);
			return NULL;
		}
	}
	return NULL;
}

static long alloc_node ( struct az_dev *p ,int idx ,int len ){ // len in u64
void *ptr;
if ( len != 0  ){
 	if (p->dma_bufs[idx].len == len )     return p->dma_bufs[idx].phys;
 	else if (p->dma_bufs[idx].len != 0)
		m2sdr_free_one(&p->pdev->dev, &p->dma_bufs[idx]);

	p->dma_bufs[idx].len = len ;   // in 64

/* Pi 5 pcie-32bit-dma overlay inbound window is 2 GiB, not 4 GiB. */
dma_set_coherent_mask(&p->pdev->dev, DMA_BIT_MASK(31));
dma_set_mask(&p->pdev->dev, DMA_BIT_MASK(31));

	 p->dma_bufs[idx].drv_addr = m2sdr_alloc_dma(&p->pdev->dev, len * 8,
						     &p->dma_bufs[idx].phys,
						     &p->dma_bufs[idx].coherent);

	ptr =p->dma_bufs[idx].drv_addr ;

	if (ptr == NULL ) {	printk(KERN_NOTICE PFX ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>pci_alloc_consistent error while idx=%d  len=%d... \n" , idx,len*8); p->dma_bufs[idx].len = 0; return  0 ;}
	if (p->dma_bufs[idx].phys >= 0x80000000ULL) {
		printk(KERN_ERR PFX "DMA addr %pad above 2GiB PCIe window idx=%d\n",
		       &p->dma_bufs[idx].phys, idx);
		if (p->dma_bufs[idx].coherent)
			dma_free_coherent(&p->pdev->dev, len * 8, ptr, p->dma_bufs[idx].phys);
		else
			dma_free_noncoherent(&p->pdev->dev, len * 8, ptr,
					     p->dma_bufs[idx].phys, DMA_BIDIRECTIONAL);
		p->dma_bufs[idx].drv_addr = NULL;
		p->dma_bufs[idx].phys = 0;
		p->dma_bufs[idx].len = 0;
		p->dma_bufs[idx].coherent = 0;
		return 0;
	}
	{
		phys_addr_t page_phys = m2sdr_page_phys(ptr);
		printk(KERN_INFO PFX "alloc idx=%d bytes=%d dma=%pad page_phys=%pa virt=%px %s%s\n",
		       idx, len * 8, &p->dma_bufs[idx].phys, &page_phys, ptr,
		       m2sdr_is_pool_va(ptr) ? "pool" :
		       (p->dma_bufs[idx].coherent ? "coherent" : "noncoherent"),
		       (page_phys != (phys_addr_t)p->dma_bufs[idx].phys) ? " PHYS_MISMATCH" : "");
	}

	p->dma_bufs[idx].phys >>= 3 ; // all phys has been lsf 3bit

}

return p->dma_bufs[idx].phys; /////????????

}

static void  free_node(struct az_dev *p  ) {
	int i ; 
	printk(KERN_NOTICE PFX "I am in free_node ... \n");
	for(i=0;i<MAX_BUFS;++i){
	if ( p->dma_bufs[i].drv_addr==NULL) continue ;	
	printk(KERN_NOTICE PFX "freeing idx=%d ... \n" , i);
	m2sdr_free_one(&p->pdev->dev, &p->dma_bufs[i]);
	}
	m2sdr_release_holds(&p->pdev->dev);
}



static void init_node (struct az_dev *p ){
	int i ;
	for(i=0;i<MAX_BUFS;++i){
		p->dma_bufs[i].drv_addr = NULL;
		p->dma_bufs[i].phys = (dma_addr_t)NULL ;
		p->dma_bufs[i].len = 0 ;
		p->dma_bufs[i].coherent = 0 ;
	} 
}

 

long az_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    
int  r ;
    unsigned int j,t,*p = (unsigned int *)arg  ;
    unsigned char cmd_id, cmd_arg8 ;
    u16 cmd_arg16 ;
    t = cmd >>  24 ;
    cmd_id = t & 0xff ;
    t = cmd >> 16 ;
    t &= 0xff ;
    cmd_arg8 = t & 0xff ;  // ID 
    cmd_arg16 = cmd & 0xffff ; //IDX 
if (cmd_id == 10)  return alloc_node(d, cmd_arg16 ,  arg ) ;   
return 0 ; 

}






int az_open(struct inode *inode, struct file *filp)
{
    return 0;
}

int az_release(struct inode *inode, struct file *filp)
{
	   printk(KERN_NOTICE PFX "az_release...\n");
    return 0;
}




static ssize_t az_read1(struct file *File, char __user *user_buffer, size_t count, loff_t *offs)
{
    int to_copy_in_bytes, copied;
 int r,t;
    if ( count  == 0   ) 
	{ 
     r = atomic_read(&rd_cond) ; 
	 if (r <=0 ) {		 
		 t = wait_event_interruptible_timeout(waitqueue, atomic_read(&rd_cond) >=1 , HZ/2 );
		 if (t==0) return 0;//time out 
	 }
    		r = atomic_read(&rd_cond) ; 
		while(  t=r,t--)atomic_dec(&rd_cond) ; //atomic_load( &rd_cond , 0)	;
		return r;
    }
	 return  2  ;
}


static ssize_t az_read(struct file *File, char __user *user_buffer, size_t count, loff_t *offs)
{
    int to_copy_in_bytes = count;
 char *p8 ;  
int idx = * offs ;

 int r,t;
    if ( count  == 0  && user_buffer==NULL  ) 
	
	{ 
     /* libpcie's do_cb drains every pending completion per wakeup, so consume
	    all queued IRQ notifications at once. On timeout return 1 anyway so
	    do_cb re-reads the pending count (BAR0 0x1c): r25 returned 0 here and
	    a missed MSI left completions stranded until the next one. */
	 if (atomic_read(&rd_cond) <= 0)
		 wait_event_interruptible_timeout(waitqueue, atomic_read(&rd_cond) >= 1,
						  msecs_to_jiffies(poll_ms));
	 atomic_set(&rd_cond, 0);
     return  1   ;
    }else {
	if (idx < 0 || idx >= MAX_BUFS || !d->dma_bufs[idx].drv_addr ||
	    count > (size_t)d->dma_bufs[idx].len * 8)
		return -EINVAL;
	if (d->dma_bufs[idx].drv_addr && !d->dma_bufs[idx].coherent)
		dma_sync_single_for_cpu(&d->pdev->dev, d->dma_bufs[idx].phys << 3,
					d->dma_bufs[idx].len * 8, DMA_FROM_DEVICE);
	if (copy_to_user ( user_buffer ,   d->dma_bufs[idx].drv_addr ,to_copy_in_bytes  ))
		return -EFAULT;
 
}
	 return  2  ;
	
}

static ssize_t az_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs)
{
int to_copy_in_bytes = count;
char *p8 ;  
int idx = * offs ;
if (idx < 0 || idx >= MAX_BUFS || !d->dma_bufs[idx].drv_addr ||
    count > (size_t)d->dma_bufs[idx].len * 8)
	return -EINVAL;
if (copy_from_user (  d->dma_bufs[idx].drv_addr , user_buffer , to_copy_in_bytes  ))
	return -EFAULT;
if (d->dma_bufs[idx].drv_addr && !d->dma_bufs[idx].coherent)
	dma_sync_single_for_device(&d->pdev->dev, d->dma_bufs[idx].phys << 3,
				   d->dma_bufs[idx].len * 8, DMA_TO_DEVICE);
return to_copy_in_bytes;
}


static unsigned int mydev_poll(struct file *filp, poll_table *wait)
{
  //  struct mydev_device *dev = filp->private_data;
   unsigned int mask = 0;
    
 //   poll_wait(filp, &dev->read_waitq, wait);
   // poll_wait(filp, &dev->write_waitq, wait);
	   printk(KERN_NOTICE PFX "mydev_poll.\n"); 
     int r = atomic_read(&poll_cond) ; 
	 if (r ==0 )	wait_event(waitqueue, atomic_read(&poll_cond) >=1 );
	 atomic_dec(&poll_cond) ;
	 m2sdr_sync_all(d, DMA_BIDIRECTIONAL);
	 mask |= POLLIN | POLLRDNORM | 1<<1;
	 
	   printk(KERN_NOTICE PFX "mydev_poll mask=%08x \n",mask); 

         return  mask   ;

}



static void xtrxfd_vma_open(struct vm_area_struct *vma)
{
    printk(KERN_NOTICE PFX "VMA open, virt %lx, phys %lx\n",vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);

}

static void xtrxfd_vma_close(struct vm_area_struct *vma)
{
    printk(KERN_NOTICE PFX "VMA close.\n");
}



static struct vm_operations_struct xtrxfd_remap_vm_ops = {
    .open =  xtrxfd_vma_open,
    .close = xtrxfd_vma_close,
};

static int  mmap_23( struct az_dev *p,struct vm_area_struct *vma);

static int  mmap_01(struct vm_area_struct *vma) ;



static int az_mmap(struct file *file, struct vm_area_struct *vma) {
    /*
    0: bar0 reg
    1: bar1 reg
    2: buf_h2c
    3: buf_c2h
    */
    int off_4k = vma->vm_pgoff  ;//= virt_to_phys(my_data) >> PAGE_SHIFT;
    int bar = off_4k;

 
    if (off_4k ==0||off_4k==1)   		return mmap_01( vma );
    else if (off_4k == 2 ||off_4k == 3 )  return mmap_23(d, vma ) ;//  0826 
    return -EINVAL;
}


////////////////////////////////////////////////////////////

static int  mmap_23( struct az_dev *p,struct vm_area_struct *vma) { //should io_ctrl id=10 before using .
    /*
    2: buf_h2c
    3: buf_c2h
    */
    unsigned long pfn, off;
    unsigned i;
    int ret;
    struct az_dmabuf_nfo *pbufs = p->dma_bufs  ;

/////////////////////////////////////////////////////////////////////////////////////////////////
 
 #if  LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
 vm_flags_set(vma,VM_LOCKED) ;
 #else 
 vma->vm_flags |= VM_LOCKED; 
 #endif  
	printk(KERN_INFO PFX "mmap_23 vma=%lx-%lx len=%lu pgoff=%lx\n",
	       vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start, vma->vm_pgoff);
    for (i =0, off = 0; i < MAX_BUFS ; ++i ) {//MAX_BUFS
		if ( pbufs[i].drv_addr == NULL )  return 0;
		if (vma->vm_start + off >= vma->vm_end)
			return 0;
		
#ifdef VA_DMA_ADDR_FIXUP
        void *va = phys_to_virt(dma_to_phys(&xtrxdev->pdev->dev, pbufs[i].phys));
#else
        void *va = pbufs[i].drv_addr;
#endif

if (va==NULL)  {
    printk(KERN_INFO PFX "mmap_23 start i=%d va==NULL !\n" , i  ); 
return 0;
}
	{
		dma_addr_t dma = (dma_addr_t)pbufs[i].phys << 3;
		unsigned long dma_pfn = dma >> PAGE_SHIFT;
		phys_addr_t page_phys = m2sdr_page_phys(va);
		/* Map the CPU pages backing drv_addr (vmalloc_to_page). FPGA is
		 * programmed with dma; log if that is not the same physical page. */
		pfn = m2sdr_page_pfn(va);
		if (!pfn)
			pfn = dma_pfn;
		if (i < 4)
			printk(KERN_INFO PFX "mmap_23 i=%d dma=%pad page_phys=%pa dma_pfn=0x%lx page_pfn=0x%lx bytes=%d%s\n",
			       i, &dma, &page_phys, dma_pfn, pfn, pbufs[i].len * 8,
			       (dma_pfn != pfn) ? " PFN_MISMATCH" : "");
	}
#if defined(__arm__) || defined(__aarch64__)
        vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);
#endif

	{
		unsigned long map_len = pbufs[i].len * 8;
		unsigned long remain = vma->vm_end - (vma->vm_start + off);
		if (map_len > remain)
			map_len = remain;
		ret = remap_pfn_range(vma, vma->vm_start + off, pfn, map_len,
				      vma->vm_page_prot);
		if (ret) {
			printk(KERN_ERR PFX "mmap_23 remap i=%d pfn=0x%lx len=%lu ret=%d\n",
			       i, pfn, map_len, ret);
			return ret;
		}
		off += map_len;
	} 
    }
    return 0 ;
}
///////////////////////////////////////////////////////////////////////////////////


static int  mmap_01(struct vm_area_struct *vma) {

    int status;     /* pci device */
    unsigned long pfn;

    int off_4k = vma->vm_pgoff  ;//= virt_to_phys(my_data) >> PAGE_SHIFT;
    int bar = off_4k;
    vma->vm_page_prot = pgprot_device(vma->vm_page_prot);
	
  
 #if  LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
 vm_flags_set(vma,VM_IO) ;
 #else 
 vma->vm_flags |= VM_IO; 
 #endif 
   
	
	pfn = pci_resource_start(d->pdev, bar) >> PAGE_SHIFT;
    if (io_remap_pfn_range(vma, vma->vm_start, pfn,
                           vma->vm_end - vma->vm_start,
                           vma->vm_page_prot))
        return -EAGAIN;
    vma->vm_ops = &xtrxfd_remap_vm_ops;
    xtrxfd_vma_open(vma);
    return 0;

}



static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open = az_open,
    .read = az_read,
    .write   = az_write,
    .release = az_release,
    .mmap = az_mmap,//2025
    .unlocked_ioctl = az_ioctl, //ok
	.poll = mydev_poll,//  new added 2025-08-25 
};

#define SDR_NAME		"FPGA"
#define SDR_MINOR_COUNT		10

static struct cdev sdr_cdev_struct;
static dev_t sdr_cdev;
static dev_t my_device_nr;
static struct class *my_class;
#define DRIVER_CLASS "MyModuleClass"
#define DRIVER_NAME "FPGA"


static irqreturn_t my_interrupt_handler(int irq, void *dev_id) {
	static int cnt = 0 ;
   atomic_inc( &rd_cond   ) ;
   atomic_inc( &poll_cond   ) ;
   
   wake_up(&waitqueue);
   ///   printk(KERN_INFO PFX "my_interrupt_handler! %d\n",cnt++); 
   return IRQ_HANDLED;
}



int do_reg_dev(void)
{
    if(alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
        printk(KERN_INFO PFX "Device m2sdr could not be allocated!\n");
        return -1;
    }
    printk(KERN_INFO PFX "read_write - Device m2sdr Major: %d, Minor: %d was registered!\n", my_device_nr >> 20, my_device_nr & 0xfffff);

   //25
   
 #if  LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0)
    if((my_class = class_create( DRIVER_CLASS)) == NULL) 
 #else 
    if((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) 
 #endif 
  
 
    {
        printk(KERN_INFO PFX "Device class can not be created!\n");
        goto ClassError;
    }
    if(device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
        printk(KERN_INFO PFX "Can not create device file!\n");
        goto FileError;
    }
    /* Initialize device file */
    cdev_init(&my_device, &fops);
    /* Regisering device to kernel */
    if(cdev_add(&my_device, my_device_nr, 1) == -1) {
        printk(KERN_INFO PFX "Registering of device to kernel failed!\n");
        goto AddError;
    }
    return 0;
AddError:
    device_destroy(my_class, my_device_nr);
FileError:
    class_destroy(my_class);
ClassError:
    unregister_chrdev_region(my_device_nr, 1);
    return 0;
}

static int az_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
    void __iomem* bar0_addr;
    void __iomem* bar1_addr;
    atomic_set( &rd_cond,0 ) ; 
    atomic_set( &poll_cond,0 ) ;
    init_waitqueue_head(&waitqueue); 
    printk(KERN_INFO PFX "rtl_pice az_probe");
    
    int err  = pci_enable_device(pdev);
    if(err) {

        dev_err(&pdev->dev, "Cannot enable PCI device, "
                "aborting.\n");
        return err;
    }
    pcie_capability_clear_and_set_word(pdev, PCI_EXP_DEVCTL,
                                       PCI_EXP_DEVCTL_READRQ, PCI_EXP_DEVCTL_READRQ_4096B);
    pci_set_master(pdev); 
    
    
    #ifdef LO_KER 
    if(pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(31))) 
    #else 
    if(dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(31))) 
    #endif 
    
    {
        dev_err(&pdev->dev, "No suitable consistent DMA available.\n");
        goto err_disable_pdev;
    }
    /*
     * Check for BARs.  We expect 0: 4KB
     */
    if(!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM) ||
            pci_resource_len(pdev, 0) < 1 << 10) {
        dev_err(&pdev->dev, "Missing UL BAR, aborting.\n");
        err = -ENODEV;
        goto err_disable_pdev;
    } //获取bar0
    err = pci_request_regions(pdev, DRV_NAME);
    if(err) {
        dev_err(&pdev->dev, "Cannot obtain PCI resources, "
                "aborting.\n");
        goto err_disable_pdev;
    }//开始获取资源
    bar0_addr = pci_iomap(pdev, 0,  1 << 15);
    if(!bar0_addr) {
        dev_err(&pdev->dev, "Failed to map BAR 0.\n");
        goto err_free_res;
    }
    bar1_addr = pci_iomap(pdev, 1,  1 << 15);
    if(!bar0_addr) {
        dev_err(&pdev->dev, "Failed to map BAR 1.\n");
        goto err_unmap0;
    }


    d = kzalloc(sizeof(struct az_dev), GFP_KERNEL);
///////////////////////////////////////////////////////////////////////////////
// 加入中断模式
    int ret;
    // 设置设备为 MSI 模式
    ret = pci_enable_msi(pdev);
    if (ret < 0) {
        printk(KERN_ERR "Failed to enable MSI\n");
        return ret;
    }

    ret = request_irq(pdev->irq, my_interrupt_handler, 0, "m2sdr_int", pdev);
    if (ret < 0) {
        printk(KERN_ERR "Failed to request IRQ\n");
        pci_disable_msi(pdev);  // 释放 MSI
        return ret;
    }
///////////////////////////////////////////////////////////////////////////////

    if(!d) {
        dev_err(&pdev->dev, "Failed to allocate memory.\n");
        err = -ENOMEM;
        goto err_unmap1;
    }

    d -> next = NULL;
    d -> pdev = pdev ;
    d->bar0_addr = bar0_addr ;
    d->bar1_addr = bar1_addr ; 
	init_node(d);
    struct az_dev **p;
    p = &az_dev_root ;
    while(*p != NULL) *p = (*p)->next;
    *p = d;

    do_reg_dev();
    
    
    return 0 ;

err_unmap1:
    pci_iounmap(pdev, bar1_addr);
err_unmap0:


    pci_iounmap(pdev, bar0_addr);
err_free_res:
    pci_release_regions(pdev);

err_disable_pdev:
    pci_clear_master(pdev); /* Nobody seems to do this */

    pci_disable_device(pdev);
    pci_set_drvdata(pdev, NULL);
    return err;
}

/* @brief This function is called, when a pci device is unregistered from the kernel
  *
  *@param dev
*/
static void az_remove(struct pci_dev *pdev)
{
    /* Stop DMA and the IRQ before freeing buffers the FPGA may still hold
       descriptors for. */
    pci_clear_master(pdev);
    free_irq(pdev->irq,pdev);
    pci_disable_msi(pdev);
	free_node(d);
//	printk(KERN_INFO PFX "az_remove 4\n");
 

    struct az_dev **ptr = &az_dev_root ; 
	
	/*
    while(*ptr != NULL) {
        kfree(*ptr);
        *ptr = (*ptr)->next;
    }
	*/
	
  //  printk(KERN_INFO PFX "az_remove 5\n");
    cdev_del(&my_device);
    device_destroy(my_class, my_device_nr);
    class_destroy(my_class);
    unregister_chrdev_region(my_device_nr, 1);
    printk(KERN_INFO PFX "Goodbye, Kernel\n");
    pci_iounmap(pdev, d->bar1_addr);
    pci_iounmap(pdev, d->bar0_addr);
  //  printk(KERN_INFO PFX "az_remove 6\n");
    pci_release_regions(pdev);
    pci_clear_master(pdev); /* Nobody seems to do this */
    pci_disable_device(pdev);
  //  printk(KERN_INFO PFX "az_remove 7\n");
    pci_set_drvdata(pdev, NULL);
}

/*PCI driver struct*/

static struct pci_driver az_driver = {
    .name = "FPGA_PCIE",
    .id_table = az_ids,
    .probe = az_probe,
    .remove = az_remove
};


/*This function is called when the module is loaded into the kernel*/
static int __init my_init(void)
{
    printk(KERN_INFO PFX "tqpcie - Registering the PCI device \n");
    pci_register_driver(&az_driver);
    return 0;
}

/*This function is called when the module is removed from the kernel*/

static void __exit my_exit(void)
{
    printk("tqpcie - Unregistering the PCI device \n");
    pci_unregister_driver(&az_driver);
	
    printk("tqpcie - Unregistering the PCI device \n");
}

module_init(my_init);
module_exit(my_exit);


