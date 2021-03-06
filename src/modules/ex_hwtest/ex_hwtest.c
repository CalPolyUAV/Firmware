#include <nuttx/config.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <systemlib/err.h>
#include <systemlib/perf_counter.h>
#include <systemlib/systemlib.h>
#include <time.h>
#include <drivers/drv_hrt.h>
#include <uORB/uORB.h>
#include <uORB/topics/actuator_controls.h>
#include <uORB/topics/actuator_controls_0.h>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/vehicle_attitude_setpoint.h>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/vehicle_status.h>

__EXPORT int ex_hwtest_main(int argc, char *argv[]);

int ex_hwtest_main(int argc, char *argv[]) {
   bool check_status = false;
   bool test_pwm = false;
   bool test_att = false;
   bool test_fm = false;
   char test_att_flags = 0x00;

   // Check arguments for syntax
   if(argc == 2) {
      if(strcmp(argv[1], "--att") == 0) {
         test_att = true;
      }
      else if(strcmp(argv[1], "--fm") == 0) {
         test_fm = true;
      }
      else if(strcmp(argv[1], "--status") == 0) {
         check_status = true;
      }
   }
   else if(argc > 2) {
      if(strcmp(argv[1], "--pwm") == 0) {
         test_pwm = true;

         for(int i = 2; i < argc; i++) {
            for(unsigned int j = 0; j < strlen(argv[i]); j++) {
               switch(argv[i][j]) {
                  case 'r':
                     test_att_flags |= 0x01;
                     break;
                  case 'p':
                     test_att_flags |= 0x02;
                     break;
                  case 'y':
                     test_att_flags |= 0x04;
                     break;
                  case 't':
                     test_att_flags |= 0x08;
                     break;
                  case '-':
                  default:
                     break;
               }
            }
         }
      }
   }
   else {
      warnx("Usage: ex_hwtest --<argument>");
      warnx("\t--pwm -<r,p,y,t> (test motor control)");
      warnx("\t--fm (test fm)");
      warnx("\t--att (test attitude sensors)");
      return 0;
   }


   // Test the PWM output to the motors
   if(test_pwm) {
      warnx("Remember to stop commander!");
      warnx("\t<commander stop>");
      warnx("Remember to link the mixer!");
      warnx("\t<mixer load /dev/pwm_output0 /etc/mixers/quad_x.main.mix>");

      // Initialize the struct for passing control information to the motors
      struct actuator_controls_s actuators;
      memset(&actuators, 0, sizeof(actuators));

      // Indicate that we'll be publishing control information for the motors
      orb_advert_t actuator_pub_fd = 
       orb_advertise(ORB_ID(actuator_controls_0), &actuators);
   
      // Initialize the struct for passing arming information
      struct actuator_armed_s arm;
      memset(&arm, 0, sizeof(arm));

      // Indicate that we'll be publishing arming information for the motors
      orb_advert_t arm_pub_fd = orb_advertise(ORB_ID(actuator_armed), &arm);
      
      // Arm the motors
      arm.timestamp = hrt_absolute_time();
      arm.ready_to_arm = true;
      arm.armed = true;

      // Publish the arming information, indicating new data is available
      orb_publish(ORB_ID(actuator_armed), arm_pub_fd, &arm);

      // Subscribe to the information we just subscribed to verify it
      int arm_sub_fd = orb_subscribe(ORB_ID(actuator_armed));

      // Read the arming information in from uORB into our struct
      orb_copy(ORB_ID(actuator_armed), arm_sub_fd, &arm);

      // Confirm arming of the motors
      if(arm.ready_to_arm && arm.armed) {
         warnx("Actuators armed");
      }
      else {
         errx(1, "Arming actuators failed");
      }

      hrt_abstime stime;
      unsigned int count = 0;
      unsigned int test = 0;

      switch(test_att_flags) {
         case 1:
            test = 0;
            break;
         case 2:
            test = 1;
            break;
         case 4:
            test = 2;
            break;
         case 8:
            test = 3;
            break;
         default:
            break;
      }

      // Incrementally increase output to the motors every 5 seconds
      while(count <= 45) {
         stime = hrt_absolute_time();
         
         if(test != 3)
            actuators.control[3] = 0.5f;

         while(hrt_absolute_time() - stime < 1000000) {
            if(count <= 5) {
               actuators.control[test] = -1.0f;
            }
            else if(count <= 10) {
               actuators.control[test] = -0.7f;
            }
            else if(count <= 15) {
               actuators.control[test] = -0.5f;
            }
            else if(count <= 20) {
               actuators.control[test] = -0.3f;
            }
            else if(count <= 25) {
               actuators.control[test] = 0.0f;
            }
            else if(count <= 30) {
               actuators.control[test] = 0.3f;
            }
            else if(count <= 35) {
               actuators.control[test] = 0.5f;
            }
            else if(count <= 40) {
               actuators.control[test] = 0.7f;
            }
            
            // Publish the control information
            actuators.timestamp = hrt_absolute_time();
            orb_publish(ORB_ID(actuator_controls_0), actuator_pub_fd, &actuators);
            usleep(500);
         }

         warnx("count %i", count);
         count++;
      }
   }

   // Test reading from the attitude sensors
   else if(test_att) {

      // Intialize the struct for reading attitude information
      struct vehicle_attitude_s att;
      memset(&att, 0, sizeof(att));

      // Subscribe to the topic that publishes attitude information
      int att_sub = orb_subscribe(ORB_ID(vehicle_attitude));

      // Setup the loop that checks for new information
      struct pollfd fds[1] = {
         { .fd = att_sub, .events = POLLIN}
      };

      hrt_abstime stime;

      stime = hrt_absolute_time();

      // Run for 30 seconds
      while(hrt_absolute_time() - stime < 30000000) {

         // Wait for sensor or parameter update.
         int ret = poll(fds, 1, 500);

         // Error
         if(ret < 0) {
            warnx("poll error");
         }
         // No new data, ignore
         else if(ret == 0) {
         }
         // New data received
         else {
            // Copy the sensor data into our struct
            if (fds[0].revents & POLLIN) {
               orb_copy(ORB_ID(vehicle_attitude), att_sub, &att);

               printf("roll: %d.%.3d\tpitch: %d.%.3d\tyaw: %d.%.3d\n",
                  (int)att.roll, (int)((att.roll-(int)att.roll)*1000),
                  (int)att.pitch, (int)((att.pitch-(int)att.pitch)*1000),
                  (int)att.yaw, (int)((att.yaw-(int)att.yaw)*1000));
            }
         }
      }
   }

   else if(test_fm || check_status) {
      uint8_t main_mode = 0;
      uint8_t sub_mode = 0;
      uint32_t custom_mode = 0;

      struct vehicle_status_s vstatus;
      memset(&vstatus, 0, sizeof(vstatus));
      
      int vstatus_sub_fd = orb_subscribe(ORB_ID(vehicle_status));

      struct vehicle_command_s vcmd;
      memset(&vcmd, 0, sizeof(vcmd));

      orb_advert_t vcmd_pub_fd = 
       orb_advertise(ORB_ID(vehicle_command), &vcmd);

      /* Don't arm if we're just checking the vehicle status */
      if(test_fm) {
         struct actuator_armed_s arm;
         memset(&arm, 0, sizeof(arm));

         orb_advert_t arm_pub_fd = orb_advertise(ORB_ID(actuator_armed), &arm);
      
         arm.timestamp = hrt_absolute_time();
         arm.ready_to_arm = true;
         arm.armed = true;

         /* Publish the arming information, indicating new data is available */
         orb_publish(ORB_ID(actuator_armed), arm_pub_fd, &arm);
      }

      orb_copy(ORB_ID(vehicle_status), vstatus_sub_fd, &vstatus);
      printf("VEHICLE INFORMATION\n");
      printf("\tmain state: %d\n", vstatus.main_state);
      printf("\tnav state: %d\n", vstatus.nav_state);
      printf("\tarming state: %d\n", vstatus.arming_state);
      printf("\thil state: %d\n", vstatus.hil_state);

      if(check_status) {
         printf("VEHICLE CONDITION\n");
         printf("\tValid voltage: %s\n",
          vstatus.condition_battery_voltage_valid ? "yes" : "no");
         printf("\tSensors initialized: %s\n", 
          vstatus.condition_system_sensors_initialized ? "yes" : "no");
         printf("\tPosition valid: %s\n",
          vstatus.condition_global_position_valid ? "yes" : "no" );
         printf("\tHome valid: %s\n",
          vstatus.condition_home_position_valid ? "yes" : "no" );
      }

      main_mode = 1;
      sub_mode = 2;

      custom_mode = (main_mode << 8) | (sub_mode);

      vcmd.param2 = custom_mode;
      vcmd.command = VEHICLE_CMD_DO_SET_MODE;

      orb_publish(ORB_ID(vehicle_command), vcmd_pub_fd, &vcmd);

      usleep(1000);
     
      orb_copy(ORB_ID(vehicle_status), vstatus_sub_fd, &vstatus);
      printf("vehicle information\n");
      printf("\tmain state: %d\n", vstatus.main_state);
      printf("\tnav state: %d\n", vstatus.nav_state);
      printf("\tarming state: %d\n", vstatus.arming_state);

   }

   return 0;
}


