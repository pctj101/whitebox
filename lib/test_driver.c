#include <stdint.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <whitebox_ioctl.h>

#include "adf4351.h"
#include "cmx991.h"
#include "whitebox_test.h"

#define WHITEBOX_DEV "/dev/whitebox"

int whitebox_parameter_set(const char *param, int value)
{
    char name[512];
    char final_value[128];
    int fd;
    snprintf(name, 512, "/sys/module/whitebox/parameters/whitebox_%s", param);
    snprintf(final_value, 128, "%d\n", value);
    fd = open(name, O_WRONLY);
    if (fd < 0)
        return fd;
    if (write(fd, final_value, strlen(final_value)+1) < 0) {
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}

int whitebox_parameter_get(const char *param)
{
    char name[512];
    char final_value[128];
    int fd;
    snprintf(name, 512, "/sys/module/whitebox/parameters/whitebox_%s", param);
    fd = open(name, O_RDONLY);
    if (fd < 0)
        return fd;
    if (read(fd, &final_value, 127) < 0) {
        close(fd);
        return 1;
    }
    close(fd);
    return atoi(final_value);
}

int test_blocking_open_close(void* data) {
    int fd;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    close(fd);
    return 0;
}

int test_blocking_open_busy(void* data) {
    int fd;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    assert(open(WHITEBOX_DEV, O_WRONLY) < 0);
    close(fd);
    return 0;
}

int test_ioctl_reset(void* data) {
    int fd;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, W_RESET);
    close(fd);
    return 0;
}

int test_ioctl_not_locked(void *data) {
    int fd;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    assert(!ioctl(fd, W_LOCKED));
    close(fd);
    return 0;
}

int test_ioctl_exciter(void *data) {
    int fd;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, WE_CLEAR);
    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 100;
    w.flags.exciter.fcw = 32;
    ioctl(fd, WE_SET, &w);
    ioctl(fd, WE_GET, &w);
    assert(w.flags.exciter.interp == 100);
    assert(w.flags.exciter.fcw == 32);
    close(fd);
    return 0;
}

int test_ioctl_fir(void *data) {
    int fd;
    int i;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, WE_CLEAR);
    ioctl(fd, WF_GET, &w);
    w.flags.fir.bank = 0;
    w.flags.fir.n = WF_COEFF_COUNT - 1;
    for (i = 0; i < WF_COEFF_COUNT - 1; ++i)
        w.flags.fir.coeff[i] = i - (WF_COEFF_COUNT >> 1) + 1;
    ioctl(fd, WF_SET, &w);
    ioctl(fd, WF_GET, &w);
    assert(w.flags.fir.bank == 0);
    assert(w.flags.fir.n == WF_COEFF_COUNT - 1);
    for (i = 0; i < WF_COEFF_COUNT - 1; ++i)
        assert(w.flags.fir.coeff[i] == i - (WF_COEFF_COUNT >> 1) + 1);
    close(fd);
    return 0;
}

int test_ioctl_cmx991(void *data) {
    int fd;
    whitebox_args_t w;
    cmx991_t cmx991;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, W_RESET);
    ioctl(fd, WC_GET, &w);
    cmx991_ioctl_get(&cmx991, &w);
    cmx991.tx_mix_pwr = TX_MIX_PWR_POWER_UP;
    cmx991_ioctl_set(&cmx991, &w);
    ioctl(fd, WC_SET, &w);
    ioctl(fd, WC_GET, &w);
    cmx991_ioctl_get(&cmx991, &w);
    assert(cmx991.tx_mix_pwr == TX_MIX_PWR_POWER_UP);
    close(fd);
    return 0;
}

int test_ioctl_adf4351(void *data) {
    int fd;
    whitebox_args_t w;
    adf4351_t adf4351;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, W_RESET);
    ioctl(fd, WA_GET, &w);
    adf4351_ioctl_get(&adf4351, &w);
    adf4351.frac_12_bit = 100;
    adf4351_ioctl_set(&adf4351, &w);
    ioctl(fd, WA_SET, &w);
    ioctl(fd, WA_GET, &w);
    adf4351_ioctl_get(&adf4351, &w);
    assert(adf4351.frac_12_bit == 100);
    close(fd);
    return 0;
}

int test_blocking_write(void *data) {
    int fd;
    int ret;
    uint32_t buf[] = { 0x00, 0x01, 0x02, 0x03 };
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 200;
    ioctl(fd, WE_SET, &w);
    for (i = 0; i < 10; ++i) {
        ret = write(fd, buf, sizeof(uint32_t) * 4);
        assert(ret == sizeof(uint32_t) * 4);
    }
    assert(fsync(fd) == 0);
    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_blocking_write_not_locked(void *data) {
    int fd;
    int ret;
    uint32_t buf[] = { 0x00, 0x01, 0x02, 0x03 };
    int i;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    assert(write(fd, buf, sizeof(uint32_t) * 4) < 0);
    close(fd);
    return 0;
}

int test_blocking_write_underrun(void *data) {
    int fd;
    int ret = 0;
    uint32_t buf[512];
    int i;
    unsigned int status;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 200;
    ioctl(fd, WE_SET, &w);

    assert(write(fd, buf, sizeof(uint32_t) * 512) ==
            sizeof(uint32_t) *512);
    w.mock_command = WMC_CAUSE_UNDERRUN;
    ioctl(fd, WM_CMD, &w);
    assert(write(fd, buf, sizeof(uint32_t) * 512) < 0);
    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return ret;
}

int test_blocking_read_overrun(void *data) {
    int fd;
    int ret = 0;
    uint32_t buf[512];
    int i;
    unsigned int status;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);
    ioctl(fd, WR_GET, &w);
    w.flags.receiver.decim = 1;
    ioctl(fd, WR_SET, &w);

    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 1;
    ioctl(fd, WE_SET, &w);
    assert(write(fd, buf, sizeof(uint32_t) * 512) ==
            sizeof(uint32_t) * 512);
    assert(fsync(fd) == 0);
    assert(read(fd, buf, sizeof(uint32_t) * 32) ==
            sizeof(uint32_t) * 32);

    w.mock_command = WMC_CAUSE_OVERRUN;
    ioctl(fd, WM_CMD, &w);
    assert(read(fd, buf, sizeof(uint32_t) * 512) < 0);
    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return ret;
}

int test_blocking_xfer(void *data) {
    int fd;
    int ret;
    uint32_t buf[4];
    uint32_t buf2[4];
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    for (i = 0; i < 4; ++i)
        buf[i] = i;

    ret = write(fd, buf, sizeof(uint32_t) * 4);
    assert(ret == sizeof(uint32_t) * 4);
    assert(fsync(fd) == 0);

    ret = read(fd, buf2, sizeof(uint32_t) * 4);
    assert(ret == sizeof(uint32_t) * 4);
    assert(fsync(fd) == 0);

    assert(memcmp(buf, buf2, sizeof(uint32_t) * 4) == 0);

    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_blocking_xfer2(void *data) {
    int fd;
    int ret;
    uint32_t buf_in1[4], buf_in2[4];
    uint32_t buf_out[8];
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    for (i = 0; i < 4; ++i) {
        buf_in1[i] = rand();
        buf_in2[i] = rand();
    }

    ret = write(fd, buf_in1, sizeof(uint32_t) * 4);
    assert(ret == sizeof(uint32_t) * 4);
    ret = write(fd, buf_in2, sizeof(uint32_t) * 4);
    assert(ret == sizeof(uint32_t) * 4);
    assert(fsync(fd) == 0);

    ret = read(fd, buf_out, sizeof(uint32_t) * 8);
    assert(ret == sizeof(uint32_t) * 8);
    assert(fsync(fd) == 0);

    assert(memcmp(buf_in1, buf_out, sizeof(uint32_t) * 4) == 0);
    assert(memcmp(buf_in2, buf_out + 4, sizeof(uint32_t) * 4) == 0);

    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_blocking_xfer3(void *data) {
    int fd;
    int ret;
    uint32_t buf_in1[128], buf_in2[128];
    uint32_t buf_out[256];
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    for (i = 0; i < 128; ++i) {
        buf_in1[i] = rand();
        buf_in2[i] = rand();
    }

    ret = write(fd, buf_in1, sizeof(uint32_t) * 128);
    assert(ret == sizeof(uint32_t) * 128);
    ret = write(fd, buf_in2, sizeof(uint32_t) * 128);
    assert(ret == sizeof(uint32_t) * 128);
    assert(fsync(fd) == 0);

    ret = read(fd, buf_out, sizeof(uint32_t) * 256);
    assert(ret == sizeof(uint32_t) * 256);
    assert(fsync(fd) == 0);

    assert(memcmp(buf_in1, buf_out, sizeof(uint32_t) * 128) == 0);
    assert(memcmp(buf_in2, buf_out + 128, sizeof(uint32_t) * 128) == 0);

    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_blocking_xfer4(void *data) {
    int fd;
    int ret;
    uint32_t buf_in1[10], buf_in2[10];
    uint32_t buf_out[20];
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    for (i = 0; i < 10; ++i) {
        buf_in1[i] = rand();
        buf_in2[i] = rand();
    }

    ret = write(fd, buf_in1, sizeof(uint32_t) * 10);
    assert(ret == sizeof(uint32_t) * 10);
    ret = write(fd, buf_in2, sizeof(uint32_t) * 10);
    assert(ret == sizeof(uint32_t) * 10);
    assert(fsync(fd) == 0);

    ret = read(fd, buf_out, sizeof(uint32_t) * 20);
    assert(ret == sizeof(uint32_t) * 20);
    assert(fsync(fd) == 0);

    assert(memcmp(buf_in1, buf_out, sizeof(uint32_t) * 10) == 0);
    assert(memcmp(buf_in2, buf_out + 10, sizeof(uint32_t) * 10) == 0);

    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_tx_fifo(void *data) {
    int fd;
    int ret;
    uint32_t sample = 0xdeadbeef;
    int i = 0;
    int quantum = whitebox_parameter_get("exciter_quantum");
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    assert(whitebox_parameter_set("loopen", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(w.flags.exciter.state & WES_SPACE);
    assert(w.flags.exciter.state & WES_AEMPTY);
    assert(!(w.flags.exciter.state & WES_DATA));
    assert(!(w.flags.exciter.state & WES_AFULL));

    assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));

    while (++i < (quantum >> 2) - 1) {
        assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
        assert(ioctl(fd, WE_GET, &w) == 0);
        assert(w.flags.exciter.state & WES_SPACE);
        assert(w.flags.exciter.state & WES_AEMPTY);
        assert(w.flags.exciter.state & WES_DATA);
        assert(!(w.flags.exciter.state & WES_AFULL));
    }

    assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(w.flags.exciter.state & WES_SPACE);
    assert(!(w.flags.exciter.state & WES_AEMPTY));
    assert(w.flags.exciter.state & WES_DATA);
    assert(!(w.flags.exciter.state & WES_AFULL));

    while (++i < WE_FIFO_SIZE - (quantum >> 2) - 1) {
        assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
        assert(ioctl(fd, WE_GET, &w) == 0);
        assert(w.flags.exciter.state & WES_SPACE);
        assert(!(w.flags.exciter.state & WES_AEMPTY));
        assert(w.flags.exciter.state & WES_DATA);
        assert(!(w.flags.exciter.state & WES_AFULL));
    }

    assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(w.flags.exciter.state & WES_SPACE);
    assert(!(w.flags.exciter.state & WES_AEMPTY));
    assert(w.flags.exciter.state & WES_DATA);
    assert(w.flags.exciter.state & WES_AFULL);

    while (++i < WE_FIFO_SIZE - 2) {
        assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
        assert(ioctl(fd, WE_GET, &w) == 0);
        assert(w.flags.exciter.state & WES_SPACE);
        assert(!(w.flags.exciter.state & WES_AEMPTY));
        assert(w.flags.exciter.state & WES_DATA);
        assert(w.flags.exciter.state & WES_AFULL);
    }

    assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(!(w.flags.exciter.state & WES_SPACE));
    assert(!(w.flags.exciter.state & WES_AEMPTY));
    assert(w.flags.exciter.state & WES_DATA);
    assert(w.flags.exciter.state & WES_AFULL);

    close(fd);

    assert(whitebox_parameter_set("loopen", 1) == 0);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_mmap_success(void *data) {
    int fd;
    int ret;
    void* rbptr;
    int tx_buffer_size = sysconf(_SC_PAGE_SIZE) << whitebox_parameter_get("user_order");
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);
    rbptr = mmap(0, tx_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(rbptr != MAP_FAILED && rbptr);
    assert(munmap(rbptr, tx_buffer_size) == 0);
    close(fd);
    return 0;
}

int test_mmap_fail(void *data) {
    int fd;
    int ret;
    int rbsize;
    void* rbptr;
    int tx_buffer_size = sysconf(_SC_PAGE_SIZE) << whitebox_parameter_get("user_source_order");
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);
    rbptr = mmap(rbptr, tx_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(rbptr == MAP_FAILED);
    rbptr = mmap(0, tx_buffer_size+sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(rbptr == MAP_FAILED);
    rbptr = mmap(0, tx_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 1);
    assert(rbptr == MAP_FAILED);
    close(fd);
    close(fd);
    return 0;
}

int test_mmap_write_success(void *data) {
    int fd;
    int ret;
    int buffer_size = sysconf(_SC_PAGE_SIZE) << whitebox_parameter_get("user_order");
    void* tx_ptr;
    whitebox_args_t w;
    uint32_t buf[] = { 0x00, 0x01, 0x02, 0x03 };
    int i;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR | O_NONBLOCK);
    assert(fd > 0);

    tx_ptr = mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(tx_ptr != MAP_FAILED && tx_ptr);
    for (i = 0; i < 2000; ++i) {
        unsigned long dest, count;
        count = ioctl(fd, W_MMAP_WRITE, &dest);
        //memcpy(dest, buf, count);
        //printf("dest=%08lx count=%08lx ", dest, count);
        if (count <= 0)
            printf("busy\n");
        if (count >= 1024) {
            ret = write(fd, 0, 1024);
            //printf("ret=%08x\n", ret);
            assert(ret == 1024);
        }
        assert(fsync(fd) == 0);
    }
    assert(munmap(tx_ptr, buffer_size) == 0);
    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

#define HUGE 256
int test_blocking_xfer_huge(void *data) {
    int fd;
    int ret;
    void *wb_ptr;
    uint32_t *tx_buf, *rx_buf;
    long tx_count, rx_count;
    int buffer_size = sysconf(_SC_PAGE_SIZE) << whitebox_parameter_get("user_order");
    int i, j;
    int t = 0, u = 0;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    wb_ptr = mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(wb_ptr != MAP_FAILED && wb_ptr);

    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 200;
    ioctl(fd, WE_SET, &w);

    for (j = 0; j < 500; ++j) {
        tx_count = ioctl(fd, W_MMAP_WRITE, &tx_buf) >> 2;
        tx_count = tx_count < HUGE ? tx_count : HUGE;
        assert(tx_count > 0);
        for (i = 0; i < tx_count; ++i) {
            *(tx_buf+i) = t++;
        }
        ret = write(fd, 0, sizeof(uint32_t) * tx_count);
        assert(ret == sizeof(uint32_t) * tx_count);

        assert(fsync(fd) == 0);

        rx_count = ioctl(fd, W_MMAP_READ, &rx_buf) >> 2;
        rx_count = rx_count < HUGE ? rx_count : HUGE;
        ret = read(fd, 0, sizeof(uint32_t) * rx_count);
        assert(ret == sizeof(uint32_t) * rx_count);
        for (i = 0; i < rx_count; ++i)
            assert(*(rx_buf+i) == u++);

        assert(fsync(fd) == 0);
    }
    assert(munmap(wb_ptr, buffer_size) == 0);
    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int main(int argc, char **argv) {
    int result;

    whitebox_parameter_set("mock_en", 1);
    whitebox_parameter_set("loopen", 1);
    whitebox_parameter_set("flow_control", 0);

    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_blocking_open_close),
        WHITEBOX_TEST(test_blocking_open_busy),
        WHITEBOX_TEST(test_blocking_write),
        WHITEBOX_TEST(test_blocking_write_not_locked),
        WHITEBOX_TEST(test_blocking_write_underrun),
        WHITEBOX_TEST(test_blocking_read_overrun),
        WHITEBOX_TEST(test_blocking_xfer),
        WHITEBOX_TEST(test_blocking_xfer2),
        WHITEBOX_TEST(test_blocking_xfer3),
        WHITEBOX_TEST(test_blocking_xfer4),
        WHITEBOX_TEST(test_ioctl_reset),
        WHITEBOX_TEST(test_ioctl_not_locked),
        WHITEBOX_TEST(test_ioctl_exciter),
        WHITEBOX_TEST(test_ioctl_fir),
        WHITEBOX_TEST(test_ioctl_cmx991),
        WHITEBOX_TEST(test_ioctl_adf4351),
        WHITEBOX_TEST(test_mmap_success),
        WHITEBOX_TEST(test_mmap_write_success),
        //WHITEBOX_TEST(test_mmap_fail),
        WHITEBOX_TEST(test_blocking_xfer_huge),
#if 0
        WHITEBOX_TEST(test_tx_fifo),
#endif
        WHITEBOX_TEST(0),
    };
    result = whitebox_test_main(tests, NULL, argc, argv);
    whitebox_parameter_set("mock_en", 0);
    whitebox_parameter_set("loopen", 0);
    whitebox_parameter_set("flow_control", 1);
    return result;
}
