#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>                 // Required for the GPIO functions
#include <linux/interrupt.h>            // Required for the IRQ code
#include <linux/kmod.h>                 // Required for executing scripts from outside
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alejandro Navarro-Soto del Castillo (a.navarrosoto)");
MODULE_DESCRIPTION("A Button/LED test driver for the BBB");
MODULE_VERSION("0.1");


static unsigned int gpioButton[4];
static unsigned int LEDs[2];
static unsigned int irqNumber[4];          ///< Used to share the IRQ number within this file
static bool ledOn[2];          ///< Is the LED on or off? Used to invert its state (off by default)
static unsigned int numberPresses[4];  ///< For information, store the number of button presses

/// Function prototype for the custom IRQ handler function -- see below for the implementation
static irq_handler_t  ebbgpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);


static int registerButton(int buttonNum) {
   gpio_request(gpioButton[buttonNum], "sysfs");       // Set up the gpioButton
   gpio_direction_input(gpioButton[buttonNum]);        // Set the button GPIO to be an input
   gpio_set_debounce(gpioButton[buttonNum], 200);      // Debounce the button with a delay of 200ms
   gpio_export(gpioButton[buttonNum], false);          // Causes gpio115 to appear in /sys/class/gpio
			                    // the bool argument prevents the direction from being changed
   // Perform a quick test to see that the button is working as expected on LKM load
   printk(KERN_INFO "ASO: The button state is currently: %d\n", gpio_get_value(gpioButton[buttonNum]));

   // GPIO numbers and IRQ numbers are not the same! This function performs the mapping for us
   irqNumber[buttonNum] = gpio_to_irq(gpioButton[buttonNum]);
   printk(KERN_INFO "ASO: The button is mapped to IRQ: %d\n", irqNumber[buttonNum]);

   // This next call requests an interrupt line
   return request_irq(irqNumber[buttonNum],             // The interrupt number requested
                        (irq_handler_t) ebbgpio_irq_handler, // The pointer to the handler function below
                        IRQF_TRIGGER_RISING,   // Interrupt on rising edge (button press, not release)
                        "ebb_gpio_handler",    // Used in /proc/interrupts to identify the owner
                        NULL);                 // The *dev_id for shared interrupt lines, NULL is okay

}


static int __init ebbgpio_init(void){
   int result = 0;
   gpioButton[0] = 8;
   gpioButton[1] = 7;
   gpioButton[2] = 12;
   gpioButton[3] = 16;

   LEDs[0] = 21;
   LEDs[1] = 26;

   numberPresses[0] = 0;
   numberPresses[1] = 0;
   numberPresses[2] = 0;
   numberPresses[3] = 0;
   printk(KERN_INFO "ASO: Initializing the ASO LKM\n");
   // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
   if (!gpio_is_valid(LEDs[0])){
      printk(KERN_INFO "ASO: invalid LED GPIO num 0\n");
      return -ENODEV;
   }
   if (!gpio_is_valid(LEDs[1])){
      printk(KERN_INFO "ASO: invalid LED GPIO num 1\n");
      return -ENODEV;
   }
   //------------------------------------------------------------------------------------------------------------
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn[0] = false;
   ledOn[1] = false;
   // LED 0
   gpio_request(LEDs[0], "sysfs");          // gpioLED is hardcoded to 49, request it
   gpio_direction_output(LEDs[0], ledOn[0]);   // Set the gpio to be in output mode and on
// gpio_set_value(gpioLED, ledOn);          // Not required as set by line above (here for reference)
   gpio_export(LEDs[0], false);             // Causes gpio49 to appear in /sys/class/gpio
			                    // the bool argument prevents the direction from being changed

    // LED 1
   gpio_request(LEDs[1], "sysfs");          // gpioLED is hardcoded to 49, request it
   gpio_direction_output(LEDs[1], ledOn[1]);   // Set the gpio to be in output mode and on
// gpio_set_value(gpioLED, ledOn);          // Not required as set by line above (here for reference)
   gpio_export(LEDs[1], false);             // Causes gpio49 to appear in /sys/class/gpio
			                    // the bool argument prevents the direction from being changed
    //------------------------------------------------------------------------------------------------------------
    registerButton(0);
    registerButton(1);
    registerButton(2);
    registerButton(3);

   //printk(KERN_INFO "ASO: The interrupt request result is: %d\n", result);
   return result;
}


static void __exit ebbgpio_exit(void){
   printk(KERN_INFO "ASO: The button D state is currently: %d\n", gpio_get_value(gpioButton[0]));
   printk(KERN_INFO "ASO: The button C state is currently: %d\n", gpio_get_value(gpioButton[1]));
   printk(KERN_INFO "ASO: The button B state is currently: %d\n", gpio_get_value(gpioButton[2]));
   printk(KERN_INFO "ASO: The button A state is currently: %d\n", gpio_get_value(gpioButton[3]));
   
   printk(KERN_INFO "ASO: The button D was pressed %d times\n", numberPresses[0]);
   printk(KERN_INFO "ASO: The button C was pressed %d times\n", numberPresses[1]);
   printk(KERN_INFO "ASO: The button B was pressed %d times\n", numberPresses[2]);
   printk(KERN_INFO "ASO: The button A was pressed %d times\n", numberPresses[3]);
   
   gpio_set_value(LEDs[0], 0);              // Turn the LED off, makes it clear the device was unloaded
   gpio_unexport(LEDs[0]);                  // Unexport the LED GPIO
   gpio_set_value(LEDs[1], 0);              // Turn the LED off, makes it clear the device was unloaded
   gpio_unexport(LEDs[1]);                  // Unexport the LED GPIO

   free_irq(irqNumber[0], NULL);               // Free the IRQ number, no *dev_id required in this case
   free_irq(irqNumber[1], NULL);               // Free the IRQ number, no *dev_id required in this case
   free_irq(irqNumber[2], NULL);               // Free the IRQ number, no *dev_id required in this case
   free_irq(irqNumber[3], NULL);               // Free the IRQ number, no *dev_id required in this case

   gpio_unexport(gpioButton[0]);               // Unexport the Button GPIO
   gpio_unexport(gpioButton[1]);               // Unexport the Button GPIO
   gpio_unexport(gpioButton[2]);               // Unexport the Button GPIO
   gpio_unexport(gpioButton[3]);               // Unexport the Button GPIO

   gpio_free(LEDs[0]);                      // Free the LED GPIO
   gpio_free(LEDs[1]);                      // Free the LED GPIO
   gpio_free(gpioButton[0]);                   // Free the Button GPIO
   gpio_free(gpioButton[1]);                   // Free the Button GPIO
   gpio_free(gpioButton[2]);                   // Free the Button GPIO
   gpio_free(gpioButton[3]);                   // Free the Button GPIO
   printk(KERN_INFO "ASO: Goodbye from the LKM!\n");
}


static irq_handler_t ebbgpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){
   if (irq == irqNumber[0]) {
        char *argv[] = { "/usr/bin/sh", "/home/pi/button1.sh", NULL };
        static char *envp[] = {
        "HOME=/",
        "TERM=linux",
        "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };

       call_usermodehelper( argv[0], argv, envp, UMH_NO_WAIT );
       
       
       ledOn[0] = false;
       gpio_set_value(LEDs[0], ledOn[0]);          // Set the physical LED accordingly
       printk(KERN_INFO "ASO: Interrupt! (button D state is %d)\n", gpio_get_value(gpioButton[0]));
       numberPresses[0]++;
       return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly

   } else if (irq == irqNumber[1]) {
        char *argv[] = { "/usr/bin/sh", "/home/pi/button2.sh", NULL };
        static char *envp[] = {
        "HOME=/",
        "TERM=linux",
        "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };

       call_usermodehelper( argv[0], argv, envp, UMH_NO_WAIT );
       
       
       ledOn[0] = true;
       gpio_set_value(LEDs[0], ledOn[0]);          // Set the physical LED accordingly
       printk(KERN_INFO "ASO: Interrupt! (button C state is %d)\n", gpio_get_value(gpioButton[1]));
       numberPresses[1]++;
       return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly

   } else if (irq == irqNumber[2]) {
        char *argv[] = { "/usr/bin/sh", "/home/pi/button3.sh", NULL };
        static char *envp[] = {
        "HOME=/",
        "TERM=linux",
        "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };

       call_usermodehelper( argv[0], argv, envp, UMH_NO_WAIT );
       ledOn[1] = false;
       gpio_set_value(LEDs[1], ledOn[1]);          // Set the physical LED accordingly
       printk(KERN_INFO "ASO: Interrupt! (button B state is %d)\n", gpio_get_value(gpioButton[2]));
       numberPresses[2]++;
       return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly

   } else if (irq == irqNumber[3]) {
        char *argv[] = { "/usr/bin/sh", "/home/pi/button4.sh", NULL };
        static char *envp[] = {
        "HOME=/",
        "TERM=linux",
        "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };
        
       ledOn[1] = true;
       gpio_set_value(LEDs[1], ledOn[1]);          // Set the physical LED accordingly
       printk(KERN_INFO "ASO: Interrupt! (button A state is %d)\n", gpio_get_value(gpioButton[3]));
       numberPresses[3]++;
       return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly
   }

   return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly
}


module_init(ebbgpio_init);
module_exit(ebbgpio_exit);