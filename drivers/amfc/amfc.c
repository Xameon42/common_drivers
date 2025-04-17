// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/irqreturn.h>
#include <linux/memcontrol.h>
#include <linux/mmzone.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/crypto.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/completion.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/syscore_ops.h>
#include <crypto/internal/scompress.h>

#ifdef CONFIG_HIGHMEM
#include <asm/fixmap.h>
#endif
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/kallsyms.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/amlogic/page_trace.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/gki_module.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/arm-smccc.h>
#include <linux/highmem.h>
#include <linux/amlogic/amfc_regs.h>
#include <linux/amlogic/amfc.h>

#undef CONFIG_AMFC_TEST
#undef CONFIG_AMFC_DEBUG

static struct amfc *amfc;
static struct page *garbage_page;

static inline int _vmalloc_or_module_addr(const void *x)
{
	/*
	 * ARM, x86-64 and sparc64 put modules in a special place,
	 * and fall back on vmalloc() if that fails. Others
	 * just put it in the vmalloc space.
	 */
#if defined(CONFIG_MODULES) && defined(MODULES_VADDR)
	unsigned long addr = (unsigned long)kasan_reset_tag(x);

	if (addr >= MODULES_VADDR && addr < MODULES_END)
		return 1;
#endif

#ifdef CONFIG_HIGHMEM
	/* special area for kmap/kmap atomic */
	if ((unsigned long)x >= FIXADDR_START && (unsigned long)x < FIXADDR_END)
		return 1;
	if ((unsigned long)x >= PKMAP_ADDR(0) && (unsigned long)x < PKMAP_ADDR(LAST_PKMAP))
		return 1;
#endif
	return is_vmalloc_addr(x);
}

static unsigned int amfc_hw_read(unsigned int reg)
{
	return readl(amfc->io_base + reg);
}

static void amfc_hw_write(unsigned int value, unsigned int reg)
{
	writel(value, amfc->io_base + reg);
}

static unsigned int amfc_clk_read(unsigned int reg)
{
	return readl(amfc->clk_base + reg);
}

/* MHz */
static unsigned int amfc_clk[] = {
	 24000000,
	166666666,
	100000000,
	800000000,
	666666666,
	500000000,
	400000000,
	285000000
};

static unsigned int init_clk __initdata;

static int early_amfc_clk_set(char *buf)
{
	int clk = 0;
	int i = 0;

	if (kstrtoint(buf, 10, &clk) != 1)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(amfc_clk); i++) {
		if (clk == amfc_clk[i]) {
			pr_emerg("set amfc_clk to %d Hz\n", clk);
			init_clk = clk;
			return 0;
		}
	}
	return -EINVAL;
}
__setup("amfc_clk=", early_amfc_clk_set);

static int amfc_hw_init(void)
{
	unsigned int value;

	/* fix 500mhz for s7d reva */
	if (is_meson_s7d_cpu() && is_meson_rev_a()) {
		pr_info("reva of s7d\n");
		amfc->rate = 500000000;
	}

	amfc->hw_version = amfc_hw_read(AMFC_GL_VERSION);
	pr_info("AMFC VLSI version:%x, feature:%x, clk:%ld\n",
		amfc->hw_version,
		amfc_hw_read(AMFC_GL_CMD0_FEATURE), amfc->rate);
	clk_set_rate(amfc->clk, amfc->rate);

	/* sw reset */
	amfc_hw_write(0x80000000, AMFC_GL_CMD0_CONTROL);
	amfc_hw_write(0, AMFC_GL_CMD0_CONTROL);
	amfc_hw_write(0, AMFC_GL_CMD1_CONTROL);
	amfc_hw_write(3, AMFC_GL_CMD0_IRQCLR);
	amfc_hw_write(3, AMFC_GL_CMD1_IRQCLR);
	amfc_hw_write(0, AMFC_GL_MISC);		// cmd0/1 mixed
	amfc_hw_write((2 << 6) | (2 << 4) | (1 << 1) | (1), AMFC_CODEC_CTRL);
	amfc_hw_write(1, AMFC_ZSTD_MODE_MISC);

	value = amfc_hw_read(AMFC_WR_MIF_CTRL);
	value &= ~(0x7 << 8);
	value |=  (2 << 8);
	amfc_hw_write(value, AMFC_WR_MIF_CTRL);

	value = amfc_hw_read(AMFC_RD_MIF_CTRL);
	value &= ~(0x7 << 8);
	value |=  (2 << 8);
	amfc_hw_write(value, AMFC_RD_MIF_CTRL);

	return 0;
}

static unsigned long vmalloc_to_phys(void *va)
{
	unsigned long pfn = vmalloc_to_pfn(va);

	WARN_ON(!pfn);
	return __pa(pfn_to_kaddr(pfn)) + offset_in_page(va);
}

static unsigned long inline get_cache_line_size(void)
{
	unsigned long ctr_el0;

	asm volatile (
	#ifdef CONFIG_ARM64
		"mrs	%[ctr_el0], ctr_el0			\n"
	#else		// ARM
		"mrc	p15, 0, %[ctr_el0], cr0, cr0, 1		\n"
	#endif
		: [ctr_el0] "+r" (ctr_el0)
		:
		: "memory", "cc"
	);
	return ctr_el0;
}

static void amfc_map_addr(unsigned long addr, unsigned long size, int dir)
{
	unsigned long ctr_el0 = get_cache_line_size();

	size = addr + size;
	// cache line size
	ctr_el0 = ((ctr_el0 >> 16) & 0xf) << 4;
	addr = addr & ~(ctr_el0 - 1);

	if (dir & DMA_FROM_DEVICE) {	// invalid
		while (addr < size) {
			asm volatile (
			#ifdef CONFIG_ARM64
				"dc	civac, %[addr]			\n"
			#else
				" mcr	p15, 0, %[addr], c7, c6, 1	\n"
			#endif
				:
				: [addr] "r" (addr)
				: "memory", "cc"
			);
			addr += ctr_el0;
		}
	} else {
		while (addr < size) {	// clean
			asm volatile (
			#ifdef CONFIG_ARM64
				"dc	cvac, %[addr]		\n"
			#else
				" mcr	p15, 0, %[addr], c7, c10, 1	\n"
			#endif
				:
				: [addr] "r" (addr)
				: "memory", "cc"
			);
			addr += ctr_el0;
		}
	}
#ifdef CONFIG_ARM64
	dsb(sy);
	dmb(sy);
#else
	dsb();
	dmb();
#endif
}

static void amfc_unmap_addr(unsigned long addr, ssize_t size, int dir)
{
	unsigned long ctr_el0 = get_cache_line_size();

	size = addr + size;
	// cache line size
	ctr_el0 = ((ctr_el0 >> 16) & 0xf) << 4;
	addr = addr & ~(ctr_el0 - 1);

	if (dir & DMA_FROM_DEVICE) {	// invalid
		while (addr < size) {
			asm volatile (
			#ifdef CONFIG_ARM64
				"dc	civac, %[addr]			\n"
			#else
				" mcr	p15, 0, %[addr], c7, c6, 1	\n"
			#endif
				:
				: [addr] "r" (addr)
				: "memory", "cc"
			);
			addr += ctr_el0;
		}
	} else {
	#if 0
		while (addr < size) {	// clean
			asm volatile (
			#ifdef CONFIG_ARM64
				"dc	cvac, %[addr]		\n"
			#else
				" mcr	p15, 0, %[addr], c7, c10, 1	\n"
			#endif
				:
				: [addr] "r" (addr)
				: "memory", "cc"
			);
			addr += ctr_el0;
		}
	#endif
	}
#ifdef CONFIG_ARM64
	dsb(sy);
	dmb(sy);
#else
	dsb();
	dmb();
#endif
}

static int show_regs(char *buf)
{
	unsigned int *p;
	unsigned int phy_base, i, size  = 0;

	phy_base = vmalloc_to_phys(amfc->io_base);
	p = (unsigned int *)amfc->io_base;
	if (buf) {
		size += sprintf(buf + size, "AMFC_REGS:");
		for (i = 0; i < 0x48; i++) {
			if (!(i & 0x3))
				size += sprintf(buf + size, "\n0x%08x: ",
						phy_base + i * 4);
			size += sprintf(buf + size, "%08x ", p[i]);
		}
		size += sprintf(buf + size, "\n");
		size += sprintf(buf + size, "CLKCTRL_AMFC_CLK_CTRL:%08x\n",
				amfc_clk_read(CLKCTRL_AMFC_CLK_CTRL));
	} else {
		pr_info("AMFC_REGS:\n");
		for (i = 0; i < 0x48; i++) {
			if (!(i & 0x3))
				pr_info("0x%08x: ", phy_base + i * 4);
			pr_cont("%08x ", p[i]);
		}
		pr_info("\n");
		pr_info("CLKCTRL_AMFC_CLK_CTRL:%08x\n",
			amfc_clk_read(CLKCTRL_AMFC_CLK_CTRL));
	}
	return size;
}

static void show_acl(struct amfc_cmd_list *acl)
{
	if (!acl)
		return;

	pr_info("acl:%px\n", acl);
	pr_info("%08x %08x %08x %08x  %08x %08x %08x %08x\n",
		acl->src_addr, acl->dst_addr, acl->link_addr, acl->src_size,
		acl->dst_size, acl->control,  acl->status,    acl->result_size);
}

#ifndef CONFIG_AMFC_DEBUG
static void dump_addr(void *buf, unsigned int size)
{
	int i;
	unsigned int *p = (unsigned int *)buf;

	pr_info("%s addr:%px, size:%d\n", __func__, buf, size);
	for (i = 0; i < size / 4; i += 4) {
		pr_info("%px: %08x %08x %08x %08x\n",
			p + i, p[i], p[i + 1], p[i + 2], p[i + 3]);
	}
}
#else
static inline void dump_addr(void *buf, unsigned int size)
{
}
#endif

/*
 *TODO:
 * 1, add debug fs
 * 2, lock protect for multi-processor
 */
static void *build_tables(unsigned int *table, void *base, ssize_t size, int stream)
{
	int i, count;
	unsigned long tmp, pfn, end = (unsigned long)base + size - 1;
	struct page *page;

	if (!table)
		return NULL;

	if (((unsigned long)base & ~PAGE_MASK) && amfc->hw_version < AMFC_VER_1_1) {
		pr_emerg("%s, base:%px not align to %ld\n", __func__, base, PAGE_SIZE);
		WARN_ON(1);
		return NULL;
	}

	/* physical contiguous in uboot */
	count = (end >> PAGE_SHIFT) - (((unsigned long)base) >> PAGE_SHIFT) + 1;
	for (i = 0; i < count; i++) {
		if (_vmalloc_or_module_addr(base)) {
			page = vmalloc_to_page(base);
			if (!page) { /* out of vmalloc range due to a new cache line added */
				pfn = page_to_pfn(garbage_page);
			} else {
				pfn = page_to_pfn(page);
			}
		} else {
			pfn = virt_to_pfn(base);
		}
		tmp = (pfn << 6) | 1;
		table[i] = tmp;
	#ifdef CONFIG_AMFC_DEBUG
		pr_info("%s, base:%px, table:%px:%x, pfn:%lx\n",
			__func__, base, table, tmp, pfn);
	#endif
		base += PAGE_SIZE;
	}
	if (stream) {
		/* avoid overflow if we force exit */
		pfn = page_to_pfn(garbage_page);
		tmp = (pfn << 6) | 1;
		table[count++] = tmp;
	}
	table[count++] = 0;	// hardware stop
	amfc_map_addr((long)table, count * 4, DMA_TO_DEVICE);

	return table;
}

static inline int get_page_count(size_t size)
{
	int num;

	size = PAGE_ALIGN(size);
	num  = size / PAGE_SIZE * sizeof(int);
	return PAGE_ALIGN(num) / PAGE_SIZE;
}

static struct page *alloc_page_table(size_t size, int type, int *order)
{
	struct page *page;
	unsigned long pages;

	pages = get_page_count(size);
	if (likely(pages == 1))
		return amfc->pages[type];

	*order = get_order(pages << PAGE_SHIFT);
	page = alloc_pages(GFP_KERNEL, *order);
	return page;
}

static void set_up_addr(struct amfc_cmd_list *acl, unsigned long addr, unsigned int off,
			int type, int isvmalloc)
{
	unsigned int low, high = 0;

	low  = addr & 0xffffffff;
#ifdef CONFIG_64BIT
	high = (addr >> 32) & 0xf;
#endif
	switch (type) {
	case ADDR_SRC:
		acl->src_addr   = low;
		acl->src_addr_h = high;
		if (amfc->hw_version > AMFC_VER_1_0)
			acl->link_addr |= (off << 16);
		break;

	case ADDR_DST:
		acl->dst_addr   = low;
		acl->dst_addr_h = high;
		if (amfc->hw_version > AMFC_VER_1_0)
			acl->link_addr |= (off);
		break;

	default:
		break;
	}
}

/* map for physical scatter address */
static void *create_map(struct amfc_cmd_list *acl, void *addr,
			ssize_t size, int stream, int type, int *order)
{
	struct page *page_table = NULL;
	void *table_addr = NULL;
	int addr_type = -1;
	int need_scatter = 1;

	if (stream)
		size -= AMFC_STREAM_MARGIN;

	/* start and end in same page */
	if ((((unsigned long)addr + size - 1) >> PAGE_SHIFT) ==
	    ((unsigned long)addr >> PAGE_SHIFT) && !stream)
		need_scatter = 0;

	if (need_scatter) {
		page_table = alloc_page_table(size, type, order);
		if (!page_table)
			return ERR_PTR(-ENOMEM);

		table_addr = build_tables(page_address(page_table), addr, size, stream);
		if (!table_addr)
			return ERR_PTR(-EINVAL);
	}

	switch (type) {
	case TABLE_SRC_DECOMPRESS:
	case TABLE_SRC_COMPRESS:
		addr_type = ADDR_SRC;
		break;

	case TABLE_DST_DECOMPRESS:
	case TABLE_DST_COMPRESS:
		addr_type = ADDR_DST;
		break;

	default:
		WARN_ON(1);
		break;
	}
	if (need_scatter) {
		set_up_addr(acl, page_to_phys(page_table), PAGE_OFF(addr), addr_type, 1);
		if (addr_type == ADDR_SRC)
			acl->src_scatter = 1;
		else if (addr_type == ADDR_DST)
			acl->dst_scatter = 1;
		return page_table;
	}
	set_up_addr(acl, vmalloc_to_phys(addr), PAGE_OFF(addr), addr_type, 1);
	return NULL;
}

/*
 * src: source buffer that need decompress
 * dst: destination buffer that need store
 * src_size: size of source buffer
 * dst_size: size for destination buffer, caller should make sure
 *           dst size is enough
 * stream: stream decompress mode
 * return value: decompressed size if success, negative value if failed
 */
int amfc_decompress(void *src, void *dst, ssize_t src_size, ssize_t dst_size, int stream)
{
	struct page *src_table = NULL, *dst_table = NULL;
	struct amfc_cmd_list *acl;
	int ret = -EINVAL;
	int src_order = 0, dst_order = 0;
	unsigned int status, control, clks = 0;
	unsigned long flags;
	unsigned long timeout = ((src_size) / PAGE_SIZE) * 50 + 5000; // us
	unsigned long long tick, cur;
	int need_copy = 0;
	void *tmp;

	if (!amfc || !amfc->decompress)
		return -ENOMEM;

	if (amfc->log > 1)
		pr_info("%s, src:%px, dst:%px, src size:%d, dst size:%d\n",
			__func__, src, dst, (int)src_size, (int)dst_size);
	if (stream)
		amfc_map_addr((long)dst, dst_size - AMFC_STREAM_MARGIN, DMA_FROM_DEVICE);
	else
		amfc_map_addr((long)dst, dst_size, DMA_FROM_DEVICE);
	amfc_map_addr((long)src, src_size, DMA_TO_DEVICE);

	if (spin_is_locked(&amfc->dec_lock))
		amfc->d_congestion++;

	if (amfc->work_mode == AMFC_POLL_MODE)
		spin_lock(&amfc->dec_lock);
	else
		spin_lock_irqsave(&amfc->dec_lock, flags);

	amfc->d_count++;
	if ((amfc->hw_version < AMFC_VER_1_1) && ((unsigned long)dst & ~PAGE_MASK) &&
	    (dst_size + ((unsigned long)dst & ~PAGE_MASK) > PAGE_SIZE)) {
		tmp = dst;
		dst = amfc->bounce_buffer;
		need_copy   = 1;
	}

	acl = amfc->decompress;
	/* setup command list, decompress use cmd0 */
	memset(acl, 0, sizeof(struct amfc_cmd_list));
	acl->src_size = src_size;
	acl->dst_size = dst_size;
	if (_vmalloc_or_module_addr(src)) {
		src_table = create_map(acl, src, src_size, 0,
				       TABLE_SRC_DECOMPRESS, &src_order);
		if (IS_ERR(src_table))
			goto out;
	} else {
		set_up_addr(acl, virt_to_phys(src), PAGE_OFF(src), ADDR_SRC, 0);
	}

	if (_vmalloc_or_module_addr(dst) || stream) {
		dst_table = create_map(acl, dst, dst_size, stream,
				       TABLE_DST_DECOMPRESS, &dst_order);
		if (IS_ERR(dst_table))
			goto out;
	} else {
		set_up_addr(acl, virt_to_phys(dst), PAGE_OFF(dst), ADDR_DST, 0);
	}
again:
	control        = 1;
	acl->algorithm = ALGORITHM_ZSTD;
	acl->end       = 1;
	acl->compress  = CMD_DECOMPRESS;
	acl->owner     = 1;
	acl->status    = 0xffffffff;
	if (!amfc->work_mode) {
		//acl->interrupt = 1;
		control |= (1 << 4);
	}

	amfc_map_addr((long)acl, sizeof(*acl), DMA_TO_DEVICE);

	amfc_hw_write(3, AMFC_GL_CMD1_IRQCLR);
	amfc_hw_write(virt_to_phys(acl) >> ADDR_SHIFT, AMFC_GL_CMD1_DESC_BASE_ADDR);
	/* trigger with irq en */
	amfc_hw_write(control, AMFC_GL_CMD1_CONTROL);

	if (!amfc->work_mode) {
		/* wait hw irq */
		ret = wait_for_completion_interruptible(&amfc->dcomp);
		if (ret) {
			ret = -EINTR;
			goto out;
		}
	} else {
		/* poll */
		tick = sched_clock();
		while (1) {
			status = amfc_hw_read(AMFC_GL_CMD1_STATUS);
			if ((status & IRQ_MASK))
				break;
			cur = sched_clock();
			if (cur - tick >= timeout * 1000) {
				pr_emerg("%s timeout:%lld -> %lld, %ld\n",
					 __func__, tick, cur, timeout);
				show_regs(NULL);
				show_acl(acl);
				if (cur - tick >= timeout * 50000)
					break;
				// init again and retry;
				amfc_hw_init();
				goto again;
			}
		}
		ret = 0;
	}
	clks = amfc_hw_read(AMFC_CMD1_TIME_MEASURE);

	amfc_unmap_addr((long)acl, sizeof(*acl), DMA_FROM_DEVICE);
	if (!IS_ERR_OR_NULL(src_table))
		amfc_unmap_addr((long)page_address(src_table),
				(ALIGN(src_size, PAGE_SIZE) / PAGE_SIZE) * sizeof(int),
				DMA_TO_DEVICE);
	if (!IS_ERR_OR_NULL(dst_table))
		amfc_unmap_addr((long)page_address(dst_table),
				(ALIGN(dst_size, PAGE_SIZE) / PAGE_SIZE) * sizeof(int),
				DMA_TO_DEVICE);
	status = amfc_hw_read(AMFC_GL_CMD1_STATUS);
	if (!(status & AMFC_ERROR_MASK)) {
		ret = acl->result_size;
	} else {
		ret = 0 - ((status & AMFC_ERROR_MASK) >> 8);
	}
out:
	if (!IS_ERR_OR_NULL(src_table) && src_table != amfc->pages[TABLE_SRC_DECOMPRESS])
		__free_pages(src_table, src_order);
	if (!IS_ERR_OR_NULL(dst_table) && dst_table != amfc->pages[TABLE_DST_DECOMPRESS])
		__free_pages(dst_table, dst_order);
	if (ret < 0) {
		if ((ret == -AMFC_DEC_DST_SIZE_OVF || ret == -AMFC_DEC_DST_PAGE_ERR) && stream) {
			while (amfc_hw_read(AMFC_WR_MIF_STATUS))
				;
			if (amfc->log)
				pr_info("decompress acl:%px, src:%px, dst:%px, src size:%5d, dstsize:%5d result size:%5d:%5d, tick:%d\n",
					acl, src, dst, (int)src_size, (int)dst_size, ret,
					acl->result_size, clks);
		} else {
			pr_err("acl:%px, decompress failed, src:%px, dst:%px, ssize:%d, dsize:%d, ret:%d, status:%x\n",
				acl, src, dst, (int)src_size, (int)dst_size,
				ret, amfc_hw_read(AMFC_GL_CMD1_STATUS));
			show_regs(NULL);
			show_acl(acl);
			if (!IS_ERR_OR_NULL(src_table))
				dump_addr(page_address(src_table), 128);
			if (!IS_ERR_OR_NULL(dst_table))
				dump_addr(page_address(dst_table), 128);
			dump_addr(src, src_size);
			dump_addr(dst, dst_size);
			need_copy = 0;	// decompress real failed
		}
		spin_lock(&amfc->com_lock);
		/* sw reset */
		amfc_hw_write(0x80000000, AMFC_GL_CMD1_CONTROL);
		spin_unlock(&amfc->com_lock);
	} else {
		amfc->din += src_size;
		if (stream) {  // separate for EROFS and ZRAM
			amfc->e_dout      += ret;
			amfc->e_dtick     += clks;
			amfc->total_dtick += clks;
		} else {
			amfc->z_dout      += ret;
			amfc->z_dtick     += clks;
			amfc->total_dtick += clks;
		}
		if (amfc->log)
			pr_info("decompress ACL:%px, src:%px, dst:%px, src size:%5d, dst size,%5d, result size:%5d, tick:%d\n",
				acl, src, dst, (int)src_size, (int)dst_size, ret, clks);
	}
	amfc_hw_write(0x03, AMFC_GL_CMD1_IRQCLR);
	if (need_copy)
		memcpy(tmp, amfc->bounce_buffer, dst_size - AMFC_STREAM_MARGIN);

	if (amfc->work_mode == AMFC_POLL_MODE)
		spin_unlock(&amfc->dec_lock);
	else
		spin_unlock_irqrestore(&amfc->dec_lock, flags);
	if (stream)
		amfc_unmap_addr((long)dst, dst_size - AMFC_STREAM_MARGIN, DMA_FROM_DEVICE);
	else
		amfc_unmap_addr((long)dst, dst_size, DMA_FROM_DEVICE);
	amfc_unmap_addr((long)src, src_size, DMA_TO_DEVICE);

	return ret;
}
EXPORT_SYMBOL(amfc_decompress);

/*
 * src: source buffer that need compress
 * dst: destination buffer that need store
 * src_size: size of source buffer
 * dst_size: size for destination buffer, caller should make sure
 *           dst size is enough
 * return value: compressed size if success, negative value if failed
 */
int amfc_compress(void *src, void *dst, ssize_t src_size, ssize_t dst_size)
{
	struct page *src_table = NULL, *dst_table = NULL;
	struct amfc_cmd_list *acl;
	int ret = -EINVAL;
	int src_order = 0, dst_order = 0;
	unsigned int status = 0, control, clks = 0;
	unsigned long flags;
	unsigned long timeout = ((src_size) / PAGE_SIZE) * 100 + 5000; // us
	unsigned long long tick, cur;

	if (!amfc || !amfc->compress)
		return -ENOMEM;

	if (amfc->log > 1)
		pr_info("%s, src:%px, dst:%px, src size:%d, dst size:%d\n",
			__func__, src, dst, (int)src_size, (int)dst_size);
	amfc_map_addr((long)src, src_size, DMA_TO_DEVICE);
	/* for compress, zram usually give size large than source, but we only
	 * need flush source size
	 */
	amfc_map_addr((long)dst, src_size, DMA_FROM_DEVICE);

	if (spin_is_locked(&amfc->com_lock))
		amfc->c_congestion++;

	if (amfc->work_mode == AMFC_POLL_MODE)
		spin_lock(&amfc->com_lock);
	else
		spin_lock_irqsave(&amfc->com_lock, flags);
	amfc->c_count++;
	acl = amfc->compress;
	/* setup command list, decompress use cmd0 */
	memset(acl, 0, sizeof(struct amfc_cmd_list));
	acl->src_size = src_size;
	acl->dst_size = dst_size;
	if (_vmalloc_or_module_addr(src)) {
		src_table = create_map(acl, src, src_size, 0,
				       TABLE_SRC_COMPRESS, &src_order);
		if (IS_ERR(src_table))
			goto out;
	} else {
		set_up_addr(acl, virt_to_phys(src), PAGE_OFF(src), ADDR_SRC, 0);
	}

	if (_vmalloc_or_module_addr(dst)) {
		dst_table = create_map(acl, dst, dst_size, 0,
				       TABLE_DST_COMPRESS, &dst_order);
		if (IS_ERR(dst_table))
			goto out;
	} else {
		set_up_addr(acl, virt_to_phys(dst), PAGE_OFF(dst), ADDR_DST, 0);
	}
again:
	control        = 1;
	acl->algorithm = ALGORITHM_ZSTD;
	acl->end       = 1;
	acl->compress  = CMD_COMPRESS;
	acl->owner     = 1;
	acl->status    = 0xffffffff;
	if (!amfc->work_mode) {
		//acl->interrupt = 1;
		control |= (1 << 4);
	}

	amfc_map_addr((long)acl, sizeof(*acl), DMA_TO_DEVICE);
	amfc_hw_write(3, AMFC_GL_CMD0_IRQCLR);
	amfc_hw_write(virt_to_phys(acl) >> ADDR_SHIFT, AMFC_GL_CMD0_DESC_BASE_ADDR);
	/* trigger with irq en */
	amfc_hw_write(control, AMFC_GL_CMD0_CONTROL);

	if (!amfc->work_mode) {
		/* wait hw irq */
		ret = wait_for_completion_interruptible(&amfc->ccomp);
		if (ret) {
			ret = -EINTR;
			goto out;
		}
	} else {
		/* poll */
		tick = sched_clock();
		while (1) {
			status = amfc_hw_read(AMFC_GL_CMD0_STATUS);
			if ((status & IRQ_MASK))
				break;
			cur = sched_clock();
			if (cur - tick >= timeout * 1000) {
				pr_emerg("%s timeout:%lld -> %lld, %ld\n",
					 __func__, tick, cur, timeout);
				show_regs(NULL);
				show_acl(acl);
				if (cur - tick >= timeout * 50000)
					break;
				// init again and retry;
				amfc_hw_init();
				goto again;
			}
		}
		ret = 0;
	}
	clks = amfc_hw_read(AMFC_CMD0_TIME_MEASURE);

	amfc_unmap_addr((long)acl, sizeof(*acl), DMA_FROM_DEVICE);
	if (!IS_ERR_OR_NULL(src_table))
		amfc_unmap_addr((long)page_address(src_table),
				(ALIGN(src_size, PAGE_SIZE) / PAGE_SIZE) * sizeof(int),
				DMA_TO_DEVICE);
	if (!IS_ERR_OR_NULL(dst_table))
		amfc_unmap_addr((long)page_address(dst_table),
				(ALIGN(dst_size, PAGE_SIZE) / PAGE_SIZE) * sizeof(int),
				DMA_TO_DEVICE);
	status = amfc_hw_read(AMFC_GL_CMD0_STATUS);
	if (!(status & AMFC_ERROR_MASK))
		ret = acl->result_size;
	else
		ret = 0 - ((status & AMFC_ERROR_MASK) >> 8);
out:
	if (!IS_ERR_OR_NULL(src_table) && src_table != amfc->pages[TABLE_SRC_COMPRESS])
		__free_pages(src_table, src_order);
	if (!IS_ERR_OR_NULL(dst_table) && dst_table != amfc->pages[TABLE_DST_COMPRESS])
		__free_pages(dst_table, dst_order);
	if (ret < 0) {
		if (((status & AMFC_ERR_MASK) >> 8) != AMFC_ENC_DST_SIZE_OVF) {
			pr_err("acl:%px, compress failed, src:%px, dst:%px, ssize:%d, dsize:%d, ret:%d, status:%x\n",
				acl, src, dst, (int)src_size, (int)dst_size,
				ret, amfc_hw_read(AMFC_GL_CMD0_STATUS));
			show_regs(NULL);
			show_acl(acl);
			spin_lock(&amfc->dec_lock);
			/* sw reset */
			amfc_hw_write(0x80000000, AMFC_GL_CMD0_CONTROL);
			spin_unlock(&amfc->dec_lock);
		}
	} else {
		amfc->cin         += src_size;
		amfc->cout        += ret;
		amfc->total_ctick += clks;
		if (amfc->log)
			pr_info("  compress acl:%px, src:%px, dst:%px, src size:%5d, result size:%5d, tick:%d\n",
				acl, src, dst, (int)src_size, ret, clks);
	}
	amfc_hw_write(0x03, AMFC_GL_CMD0_IRQCLR);
	if (amfc->work_mode == AMFC_POLL_MODE)
		spin_unlock(&amfc->com_lock);
	else
		spin_unlock_irqrestore(&amfc->com_lock, flags);

	if (ret > 0)
		amfc_unmap_addr((long)dst, ret, DMA_FROM_DEVICE);
	amfc_unmap_addr((long)src, src_size, DMA_TO_DEVICE);
	return ret;
}
EXPORT_SYMBOL(amfc_compress);

/*
 * IRQ for compress
 */
static irqreturn_t amfc0_irq_handler(int irq, void *dev_instance)
{
	struct amfc *amfc = (struct amfc *)dev_instance;
	unsigned int val;

	val = amfc_hw_read(AMFC_GL_CMD0_STATUS);
	if (val & AMFC_IRQ_FLAG) { /* job done or error */
		complete(&amfc->ccomp);
	} else {
		pr_err("AMFC0 BUG: no valid IRQ flags but irq happen:%x\n", val);
	}
	amfc_hw_write(3, AMFC_GL_CMD0_IRQCLR); /* clear IRQ */

	return IRQ_HANDLED;
}

/*
 * IRQ for decompress
 */
static irqreturn_t amfc1_irq_handler(int irq, void *dev_instance)
{
	struct amfc *amfc = (struct amfc *)dev_instance;
	unsigned int val;

	val = amfc_hw_read(AMFC_GL_CMD1_STATUS);
	if (val & AMFC_IRQ_FLAG) { /* job done or error */
		complete(&amfc->dcomp);
	} else {
		pr_err("AMFC1 BUG: no valid IRQ flags but irq happen:%x\n", val);
	}
	amfc_hw_write(3, AMFC_GL_CMD1_IRQCLR); /* clear IRQ */

	return IRQ_HANDLED;
}

/* --------------------- crypto interface --------------------- */
// TODO: implement them
static int amfc_crypto_init(struct crypto_tfm *tfm)
{
	return 0;
}

static void amfc_crypto_exit(struct crypto_tfm *tfm)
{
}

static int amfc_crypto_compress(struct crypto_tfm *tfm, const u8 *src,
			 unsigned int slen, u8 *dst, unsigned int *dlen)
{
	int ret;

	ret = amfc_compress((void *)src, dst, slen, *dlen);
	if (ret < 0)
		return ret;
	*dlen = ret;
	return 0;
}

static int amfc_crypto_decompress(struct crypto_tfm *tfm, const u8 *src,
			   unsigned int slen, u8 *dst, unsigned int *dlen)
{
	int ret;

	ret = amfc_decompress((void *)src, dst, slen, *dlen, 0);
	if (ret < 0)
		return ret;
	*dlen = ret;
	return 0;
}

static struct crypto_alg amfc_alg = {
	.cra_name		= "amfc",
	.cra_driver_name	= "amfc-generic",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= 0,
	.cra_module		= THIS_MODULE,
	.cra_init		= amfc_crypto_init,
	.cra_exit		= amfc_crypto_exit,
	.cra_u			= {
		.compress = {
			.coa_compress	= amfc_crypto_compress,
			.coa_decompress	= amfc_crypto_decompress,
		}
	}
};

static int eamfc_crypto_decompress(struct crypto_tfm *tfm, const u8 *src,
			   unsigned int slen, u8 *dst, unsigned int *dlen)
{
	int ret;
	int off;
	unsigned int out_size;

	/* special handle for EROFS */
	// align src addr to  page header
	if (slen > PAGE_SIZE) {
		off = ((unsigned long)src) & ~PAGE_MASK;
		src -= off;
		slen += off;
	}
	out_size = *dlen;
	*dlen += AMFC_STREAM_MARGIN;
	ret = amfc_decompress((void *)src, dst, slen, *dlen, 1);
	if (ret < 0 && ((ret != -AMFC_DEC_DST_SIZE_OVF) && (ret != -AMFC_DEC_DST_PAGE_ERR)))
		return ret;
	*dlen = out_size;
	return 0;
}

/* for erofs*/
static struct crypto_alg eamfc_alg = {
	.cra_name		= "eamfc",
	.cra_driver_name	= "eamfc-generic",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= 0,
	.cra_module		= THIS_MODULE,
	.cra_init		= amfc_crypto_init,
	.cra_exit		= amfc_crypto_exit,
	.cra_u			= {
		.compress = {
			.coa_compress	= amfc_crypto_compress,
			.coa_decompress	= eamfc_crypto_decompress,
		}
	}
};

static int eamfc2_crypto_decompress(struct crypto_tfm *tfm, const u8 *src,
			   unsigned int slen, u8 *dst, unsigned int *dlen)
{
	int ret;
	unsigned int out_size;

	out_size = *dlen;
	*dlen += AMFC_STREAM_MARGIN;
	ret = amfc_decompress((void *)src, dst, slen, *dlen, 1);
	if (ret < 0 && ((ret != -AMFC_DEC_DST_SIZE_OVF) && (ret != -AMFC_DEC_DST_PAGE_ERR)))
		return ret;
	*dlen = out_size;
	return 0;
}

/* for erofs*/
static struct crypto_alg eamfc2_alg = {
	.cra_name		= "eamfc2",
	.cra_driver_name	= "eamfc2-generic",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= 0,
	.cra_module		= THIS_MODULE,
	.cra_init		= amfc_crypto_init,
	.cra_exit		= amfc_crypto_exit,
	.cra_u			= {
		.compress = {
			.coa_compress	= amfc_crypto_compress,
			.coa_decompress	= eamfc2_crypto_decompress,
		}
	}
};

/*----------- debug fs layer --------------*/
static ssize_t log_store(struct class *cla,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned long value = -1UL;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;
	amfc->log = value;
	return count;
}

static ssize_t log_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "log:%d\n", amfc->log);
}
static CLASS_ATTR_RW(log);

static ssize_t statistics_store(struct class *cla,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	/* clear all statictics data */
	amfc->cin                = 0;
	amfc->cout               = 0;
	amfc->din                = 0;
	amfc->z_dout             = 0;
	amfc->e_dout             = 0;
	amfc->total_ctick        = 0;
	amfc->total_dtick        = 0;
	amfc->z_dtick            = 0;
	amfc->e_dtick            = 0;
	amfc->c_count            = 0;
	amfc->d_count            = 0;
	amfc->c_congestion       = 0;
	amfc->d_congestion       = 0;
	return count;
}

static ssize_t statistics_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	int s = 0;
	unsigned long long ctmp1, dtmp1, ctmp2, dtmp2;
	unsigned long rate = amfc->rate / 1000000;

	ctmp1 = amfc->cout * 100;
	do_div(ctmp1, amfc->cin);
	s += sprintf(buf + s, "Total compressed in:             %16lld\n", amfc->cin);
	s += sprintf(buf + s, "Total compressed out:            %16lld\n", amfc->cout);
	s += sprintf(buf + s, "Average compress ratio:          %16lld%%\n", ctmp1);
	s += sprintf(buf + s, "Total decompressed in:           %16lld\n", amfc->din);
	s += sprintf(buf + s, "Total decompressed out:          %16lld\n", amfc->z_dout + amfc->e_dout);

	ctmp1 = amfc->total_ctick;
	dtmp1 = amfc->total_dtick;
	do_div(ctmp1, rate);
	do_div(dtmp1, rate);
	s += sprintf(buf + s, "Total compressed tick(us):       %16lld\n", ctmp1);
	s += sprintf(buf + s, "Total decompressed tick(us):     %16lld\n", dtmp1);

	ctmp2 = amfc->cin;
	dtmp2 = amfc->z_dout + amfc->e_dout;
	do_div(ctmp2, ctmp1);
	do_div(dtmp2, dtmp1);
	s += sprintf(buf + s, "Average compress speed(MB/s):    %16lld\n", ctmp2);
	s += sprintf(buf + s, "Average decompress speed(MB/s):  %16lld\n", dtmp2);

	dtmp1 = amfc->z_dtick;
	do_div(dtmp1, rate);
	dtmp2 = amfc->z_dout;
	do_div(dtmp2, dtmp1);
	s += sprintf(buf + s, "Avg ZRAM decompress speed(MB/s): %16lld\n", dtmp2);

	dtmp1 = amfc->e_dtick;
	do_div(dtmp1, rate);
	dtmp2 = amfc->e_dout;
	do_div(dtmp2, dtmp1);
	s += sprintf(buf + s, "Avg EROFS decompress speed(MB/s):%16lld\n", dtmp2);

	s += sprintf(buf + s, "Clock speed(Hz):                 %16ld\n",  amfc->rate);
	s += sprintf(buf + s, "Compressed count:                %16ld\n",  amfc->c_count);
	s += sprintf(buf + s, "Decompressed count:              %16ld\n",  amfc->d_count);
	s += sprintf(buf + s, "Compress congestion count:       %16ld\n",  amfc->c_congestion);
	s += sprintf(buf + s, "Decompress congestion count:     %16ld\n",  amfc->d_congestion);

	return s;
}
static CLASS_ATTR_RW(statistics);

static ssize_t work_mode_store(struct class *cla,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned long value = -1UL;

	if (kstrtoul(buf, 16, &value))
		return -EINVAL;
	if (value >= 3) {
		pr_info("invalid input:%s\n", buf);
		return -EINVAL;
	}
	amfc->work_mode = value;
	return count;
}

static ssize_t work_mode_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "work_mode:%d\n", amfc->work_mode);
}
static CLASS_ATTR_RW(work_mode);

static ssize_t clk_store(struct class *cla,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned long value = -1UL;
	int i;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(amfc_clk); i++) {
		if (amfc_clk[i] == value)
			break;
	}
	if (i >= ARRAY_SIZE(amfc_clk)) {
		pr_emerg("unsupported clk:%ld\n", value);
		return -1;
	}
	if (is_meson_s7d_cpu() && is_meson_rev_a())
		value = 500000000;
	clk_set_rate(amfc->clk, value);
	amfc->rate = value;
	return count;
}

static ssize_t clk_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "FREQ:%ld hz\n", clk_get_rate(amfc->clk));
}
static CLASS_ATTR_RW(clk);

static ssize_t reg_dump_store(struct class *cla,
			      struct class_attribute *attr,
			      const char *buf, size_t count)
{
	unsigned int off, value;

	if (sscanf(buf, "%x %x\n", &off, &value) != 2)
		return -EINVAL;

	if (off > AMFC_MIF_QOS_UGT)
		return -EINVAL;

	amfc_hw_write(value, off);
	return count;
}

static ssize_t reg_dump_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	int size;

	size = show_regs(buf);
	return size;
}
static CLASS_ATTR_RW(reg_dump);

#ifdef CONFIG_AMFC_TEST
static char test_buf[PAGE_SIZE * 4] __aligned(4096);
static ssize_t zram_test_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	struct lruvec *lruvec;
	pg_data_t *pgdat, *tmp;
	struct page *page, *next;
	struct list_head *list;
	void *src;
	int ret, lru;
	struct mem_cgroup *root = NULL, *memcg;
	int pages = 0;

	kstrtoint(buf, 10, &pages);
	if (pages <= 0)
		return count;
	for_each_online_pgdat(pgdat) {
		memcg = mem_cgroup_iter(root, NULL, NULL);
		do {
			lruvec = mem_cgroup_lruvec(memcg, pgdat);
			tmp = lruvec_pgdat(lruvec);

			for_each_lru(lru) {
				/* only count for filecache */
				if (is_file_lru(lru) &&
				    lru != LRU_UNEVICTABLE)
					continue;

				list = &lruvec->lists[lru];
				//spin_lock(&tmp->lru_lock);
				list_for_each_entry_safe(page, next,
							 list, lru) {
					src = kmap(page);
					ret = amfc_compress(src, test_buf,
							    PAGE_SIZE, PAGE_SIZE);
					if (ret > 0)
						ret = amfc_decompress(test_buf,
								      test_buf + PAGE_SIZE,
								      ret, PAGE_SIZE);
					kunmap(page);
					pages--;
					if (pages <= 0)
						return count;
				}
				//spin_unlock(&tmp->lru_lock);
			}
		} while ((memcg =  mem_cgroup_iter(root, memcg, NULL)));
	}

	return count;
}
static CLASS_ATTR_WO(zram_test);

static ssize_t zfile_test_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int i = 0, size, ret;
	unsigned long addr;
	void *p;

	if (sscanf(buf, "%lx %x", &addr, &size) != 2)
		return -EINVAL;

	if (size < 0)
		return count;

	pr_info("%s, input addr:%lx, size:%x\n", __func__, addr, size);
	p = ioremap(addr, size);
	if (!p)
		return count;
	while (i < size) {
		ret = amfc_compress(p + i, test_buf, PAGE_SIZE, PAGE_SIZE);
		if (ret > 0)
			amfc_decompress(test_buf, test_buf + 2 * PAGE_SIZE, ret, PAGE_SIZE);
		i += PAGE_SIZE;
	}
	iounmap(p);

	return count;
}
static CLASS_ATTR_WO(zfile_test);
#endif

static struct attribute *amfc_attrs[] = {
	&class_attr_log.attr,
	&class_attr_reg_dump.attr,
	&class_attr_work_mode.attr,
	&class_attr_statistics.attr,
	&class_attr_clk.attr,
#ifdef CONFIG_AMFC_TEST
	&class_attr_zram_test.attr,
	&class_attr_zfile_test.attr,
#endif
	NULL
};
ATTRIBUTE_GROUPS(amfc);

static struct class amfc_class = {
	.name = "amfc",
	.class_groups = amfc_groups,
};

/*------------ driver layer ---------------*/
static int amfc_remove(struct platform_device *pdev)
{
	int i;
	struct amfc *tmp = amfc;

	if (amfc) {
		amfc = NULL;
		class_destroy(&amfc_class);
		for (i = 0; i < 4; i++) {
			if (tmp->pages[i])
				__free_pages(tmp->pages[i], 0);
		}
		vfree(tmp->bounce_buffer);
		kfree(tmp->compress);
		kfree(tmp->decompress);
		devm_kfree(&pdev->dev, tmp);
	}
	return 0;
}

void amfc_syscore_shutdown(void)
{
#if 0	// need always on
	struct platform_device *pdev;

	pdev = to_platform_device(amfc->dev);
	amfc_remove(pdev);
	pm_runtime_force_suspend(&pdev->dev);
#endif
}

static struct syscore_ops amfc_syscore_ops = {
	.shutdown = amfc_syscore_shutdown,
};

static int __init amfc_probe(struct platform_device *pdev)
{
	int r = 0, irq, num;
	unsigned int tmp;
	struct resource *res;
	struct device_node *node;

	amfc = devm_kzalloc(&pdev->dev, sizeof(*amfc), GFP_KERNEL);
	if (!amfc)
		return -ENOMEM;

	garbage_page = alloc_pages(GFP_KERNEL, 0);
	if (!garbage_page)
		return -ENOMEM;

	node = pdev->dev.of_node;
	amfc->dev = &pdev->dev;

	dma_set_mask(amfc->dev, DMA_BIT_MASK(36));
	amfc->compress = kzalloc(sizeof(*amfc->compress), GFP_KERNEL);
	amfc->decompress = kzalloc(sizeof(*amfc->decompress), GFP_KERNEL);
	if (!amfc->compress || !amfc->decompress) {
		r = -ENOMEM;
		goto err;
	}

	for (num = 0; num < 4; num++) {
		amfc->pages[num] = alloc_pages(GFP_KERNEL, 0);
		if (!amfc->pages[num]) {
			r = -ENOMEM;
			goto err;
		}
	}
	tmp = (unsigned long)of_device_get_match_data(&pdev->dev);
	pr_info("%s, socs:%d, coherent:%d, garbage page:%lx, bounce:%px, c:%px, d:%px\n",
		__func__, tmp, dev_is_dma_coherent(amfc->dev),
		page_to_pfn(garbage_page), amfc->bounce_buffer, amfc->compress, amfc->decompress);
	amfc->chip = tmp;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		amfc->io_base = ioremap(res->start, resource_size(res));
	if (!amfc->io_base) {
		pr_err("map io base failed:%px\n", res);
		r = -EIO;
		goto err;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res)
		amfc->clk_base = ioremap(res->start, resource_size(res));
	if (!amfc->clk_base) {
		pr_err("map clk base failed:%px\n", res);
		r = -EIO;
		goto err;
	}

	irq = platform_get_irq_byname(pdev, "amfc0");
	r = request_irq(irq, amfc0_irq_handler,
			IRQF_SHARED, "amfc0", amfc);
	if (r < 0)
		pr_err("request irq failed:%d, r:%d\n", irq, r);

	irq = platform_get_irq_byname(pdev, "amfc1");
	r = request_irq(irq, amfc1_irq_handler,
			IRQF_SHARED, "amfc1", amfc);
	if (r < 0)
		pr_err("request irq failed:%d, r:%d\n", irq, r);

	amfc->clk = devm_clk_get(&pdev->dev, NULL);
	if (!amfc->clk) {
		pr_err("%s, can't get clk\n", __func__);
		goto err;
	}
	clk_prepare_enable(amfc->clk);
	amfc->rate = clk_get_rate(amfc->clk);
	pm_runtime_enable(amfc->dev);
	r = pm_runtime_get(amfc->dev);
	if (r) {
		pr_err("get runtime pm failed:%d\n", r);
		goto err;
	}

	init_completion(&amfc->ccomp);
	init_completion(&amfc->dcomp);
	spin_lock_init(&amfc->com_lock);
	spin_lock_init(&amfc->dec_lock);
	amfc_hw_init();
	/*
	 * Test on T6D:
	 * ZRAM  average compress time:6.7us, max:16us, decompress is faster
	 * than compress of ZRAM. EROFS decomperss average time:18us, max:69us.
	 * There are too many atomic envs in upper layer caller,so can't
	 * use schedule, if with IRQ enabled when spinlock, AMFC hardware
	 * may be interrupted by other long IRQ over 7 ~ 9 ms,
	 * this will cause long waiting time of AMFC upper caller compared
	 * with IRQ disabled. So finally chose spinlock with IRQ off.
	 */
	amfc->work_mode = AMFC_POLL_IRQ_OFF;
#ifdef CONFIG_AMFC_DEBUG
	amfc->log = 1;
#endif

	if (amfc->hw_version < AMFC_VER_1_1) {
		amfc->bounce_buffer = vzalloc(128 * 1024 + AMFC_STREAM_MARGIN);
		if (!amfc->bounce_buffer)
			goto err;
	}

	r = crypto_register_alg(&amfc_alg);
	if (r) {
		pr_err("register amfc crypto failed:%d\n", r);
		goto err;
	}

	r = crypto_register_alg(&eamfc_alg);
	if (amfc->hw_version > AMFC_VER_1_0)
		r = crypto_register_alg(&eamfc2_alg);
	if (r) {
		crypto_unregister_alg(&amfc_alg);
		pr_err("register eamfc crypto failed:%d\n", r);
		goto err;
	}

	r = class_register(&amfc_class);
	if (r)
		pr_info("%s, class regist failed:%d\n", __func__, r);

	register_syscore_ops(&amfc_syscore_ops);
	return 0;
err:
	for (num = 0; num < 4; num++) {
		if (amfc->pages[num])
			__free_pages(amfc->pages[num], 0);
	}
	kfree(amfc->compress);
	kfree(amfc->decompress);
	devm_kfree(&pdev->dev, amfc);
	amfc = NULL;
	return r;
}

static int amfc_suspend(struct device *dev)
{
	clk_disable_unprepare(amfc->clk);
	return 0;
}

static int amfc_resume(struct device *dev)
{
	clk_prepare_enable(amfc->clk);
	amfc_hw_init();
	return 0;
}

static SIMPLE_DEV_PM_OPS(amfc_pm_ops, amfc_suspend, amfc_resume);

#ifdef CONFIG_OF
static const struct of_device_id amfc_match[] = {
	{
		.compatible = "amlogic,amfc-s7d",
		.data = (void *)AMFC_S7D,
	},
	{
		.compatible = "amlogic,amfc-t6d",
		.data = (void *)AMFC_T6D,
	},
	{}
};
#endif

static struct platform_driver amfc_driver = {
	.driver = {
		.name  = "amfc",
		.owner = THIS_MODULE,
		.pm = &amfc_pm_ops,
	},
	.remove   = amfc_remove,
};

int __init amfc_init(void)
{
	int ret;

#ifdef CONFIG_OF
	const struct of_device_id *match_id;
	match_id = amfc_match;
	amfc_driver.driver.of_match_table = match_id;
#endif
	ret = platform_driver_probe(&amfc_driver, amfc_probe);
	return ret;
}

void __exit amfc_exit(void)
{
	platform_driver_unregister(&amfc_driver);
}
module_init(amfc_init);
module_exit(amfc_exit);
MODULE_LICENSE("GPL");
