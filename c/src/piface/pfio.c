/**
 * pfio.c
 * functions for accessing the PiFace add-on for the Raspberry Pi
 */
#include "pfio.h"

#undef VERBOSE_MODE

static Spi * spi;

static void spi_transfer(char * txbuffer, char * rxbuffer);
static void spi_write(char port, char value);
static char spi_read(char port);


char pfio_init(void)
{
    if ((spi = malloc(sizeof(Spi))) == NULL)
        return -1;

    // initialise the spi with some values
    // create the path string
    char path[MAXPATH];
    if (snprintf(path, MAXPATH, "/dev/spidev%d.%d", SPI_BUS, SPI_DEVICE) >= MAXPATH)
    {
        fprintf(stderr, "ERROR: Bus and/or device number is invalid.");
        return -1;
    }

    // try to open the device
    if ((spi->fd = open(path, O_RDWR, 0)) < 0)
    {
        fprintf(stderr, "ERROR: Can not open device");
        return -1;
    }

    // try to control the device
    char temp;
    if (ioctl(spi->fd, SPI_IOC_RD_MODE, &temp) < 0)
    {
        fprintf(stderr, "ERROR: Can not get spi mode");
        return -1;
    }
    spi->mode = temp;

    // try to get the bits per word
    if (ioctl(spi->fd, SPI_IOC_RD_BITS_PER_WORD, &temp) < 0)
    {
        fprintf(stderr, "ERROR: Can not get bits per word");
        return -1;
    }
    spi->bitsperword = temp;

    // try to get the max speed
    int maxspeed;
    if (ioctl(spi->fd, SPI_IOC_RD_MAX_SPEED_HZ, &maxspeed) < 0)
    {
        fprintf(stderr, "ERROR: Can not get max speed hz");
        return -1;
    }
    spi->maxspeed = maxspeed;

    // set up the ports
    // fixed SPI addresses so that we don't have that annoying
    // LED flashing when initializing pfio.
    spi_write(IOCON,  8); // enable hardware addressing
    spi_write(GPIOA, 0x00); // turn on port A
    spi_write(IODIRA, 0); // set port A as an output
    spi_write(IODIRB, 0xFF); // set port B as an input
    spi_write(GPPUB, 0xFF); // turn on port B pullups

    // initialise all outputs to 0
    int i;
    for (i = 1; i <= 8; i++)
        pfio_digital_write(i, 0);

    return 0;
}

char pfio_deinit(void)
{
    close(spi->fd);
    free(spi);
    return 0;
}

char pfio_digital_read(char pin_number)
{
    char current_pin_values = pfio_read_input();
    char pin_bit_mask = pfio_get_pin_bit_mask(pin_number);
    // note: when using bitwise operators and checking if a mask is
    // in there it is always better to check if the result equals
    // to the desidered mask, in this case pin_bit_mask.
    return ( current_pin_values & pin_bit_mask ) == pin_bit_mask;
}

void pfio_digital_write(char pin_number, char value)
{
    char pin_bit_mask = pfio_get_pin_bit_mask(pin_number);
    char old_pin_values = pfio_read_output();

    char new_pin_values;
    if (value > 0)
        new_pin_values = old_pin_values | pin_bit_mask;
    else
        new_pin_values = old_pin_values & ~pin_bit_mask;

#ifdef VERBOSE_MODE
    printf("digital_write: pin number %d, value %d\n", pin_number, value);
    printf("pin bit mask: 0x%x\n", pin_bit_mask);
    printf("old pin values: 0x%x\n", old_pin_values);
    printf("new pin values: 0x%x\n", new_pin_values);
    printf("\n");
#endif

    pfio_write_output(new_pin_values);
}

char pfio_read_input(void)
{
    // XOR by 0xFF so we get the right outputs.
    // before a turned off input would read as 1,
    // confusing developers.
    return spi_read(INPUT_PORT) ^ 0xFF;
}
    
char pfio_read_output(void)
{
    return spi_read(OUTPUT_PORT);
}

void pfio_write_output(char value)
{
    spi_write(OUTPUT_PORT, value);
}

char pfio_get_pin_bit_mask(char pin_number)
{
    // removed - 1 to reflect pin numbering of
    // the python interface (0, 1, ...) instead
    // of (1, 2, ...)
    return 1 << pin_number;
}

char pfio_get_pin_number(char bit_pattern)
{
    char pin_number = 0; // assume pin 0
    while ((bit_pattern & 1) == 0)
    {
        bit_pattern >>= 1;
        if (++pin_number > 7)
        {
            pin_number = 0;
            break;
        }
    }
    return pin_number;
}


static void spi_transfer(char * txbuffer, char * rxbuffer)
{
    // set up some transfer information
    struct spi_ioc_transfer transfer_buffer = 
    {
        .tx_buf = (unsigned long) txbuffer,
        .rx_buf = (unsigned long) rxbuffer,
        .len = TRANSFER_LEN,
        .delay_usecs = TRANSFER_DELAY,
        .speed_hz = TRANSFER_SPEED,
        .bits_per_word = TRANSFER_BPW,
    };

    // actually do the transfer
    if (ioctl(spi->fd, SPI_IOC_MESSAGE(1), &transfer_buffer) < 1)
    {
        fprintf(stderr, "ERROR: Can not send SPI message");
        perror(0);
    }
}

static void spi_write(char port, char value)
{
    char txbuffer[] = {SPI_WRITE_CMD, port, value};
    char rxbuffer[ARRAY_SIZE(txbuffer)];
    spi_transfer(txbuffer, rxbuffer);
}

static char spi_read(char port)
{
    char txbuffer[] = {SPI_READ_CMD, port, 0xff};
    char rxbuffer[ARRAY_SIZE(txbuffer)];
    spi_transfer(txbuffer, rxbuffer);
    return rxbuffer[2];
}
