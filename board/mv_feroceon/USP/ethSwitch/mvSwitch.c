/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell 
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.

********************************************************************************
Marvell Commercial License Option

If you received this File from Marvell and you have entered into a commercial
license agreement (a "Commercial License") with Marvell, the File is licensed
to you under the terms of the applicable Commercial License.

********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or 
modify this File in accordance with the terms and conditions of the General 
Public License Version 2, June 1991 (the "GPL License"), a copy of which is 
available along with the File in the license.txt file or by writing to the Free 
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or 
on the worldwide web at http://www.gnu.org/licenses/gpl.txt. 

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED 
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY 
DISCLAIMED.  The GPL License provides additional details about this warranty 
disclaimer.
********************************************************************************
Marvell BSD License Option

If you received this File from Marvell, you may opt to use, redistribute and/or 
modify this File under the following licensing terms. 
Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    *   Redistributions of source code must retain the above copyright notice,
	    this list of conditions and the following disclaimer. 

    *   Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution. 

    *   Neither the name of Marvell nor the names of its contributors may be 
        used to endorse or promote products derived from this software without 
        specific prior written permission. 
    
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "mvOs.h"
#include "mvSwitch.h"
#include "eth-phy/mvEthPhy.h"
#include "mvSwitchRegs.h"
#include "mvCtrlEnvLib.h"


static void switchVlanInit(MV_U32 ethPortNum,
						   MV_U32 switchCpuPort,
					   MV_U32 switchMaxPortsNum,
					   MV_U32 switchPortsOffset,
					   MV_U32 switchEnabledPortsMask);

void mvEthSwitchRegWrite(MV_U32 ethPortNum, MV_U32 phyAddr,
                                 MV_U32 regOffs, MV_U16 data);

void mvEthSwitchRegRead(MV_U32 ethPortNum, MV_U32 phyAddr,
                             MV_U32 regOffs, MV_U16 *data);

static int	cp_is_l30(void)
{
	if (!strcmp(getenv("unitModel"), CP_L30_STR))
	{
		return 1;
	}
	return 0;
}

struct mv_port_info
{
		int dev_num;			/* The switch number (0 or 1 in CUT3) */
		int physical_port_num;	/* The physical port number on the internal switch */
};
	
#define MAX_LOGICAL_SWITCH_PORTS 9 /* Including the DMZ port */

static struct mv_port_info mv_port_map[MAX_LOGICAL_SWITCH_PORTS] =
{
		/* LAN1 */
		{ 0 , 1 },

		/* LAN2 */
		{ 0 , 3 },

		/* LAN3 */
		{ 1 , 0 },

		/* LAN4 */
		{ 1 , 2 },

		/* LAN5 */
		{ 0 , 0 },

		/* LAN6 */
		{ 0 , 2 },

		/* LAN7 */
		{ 0 , 4 },

		/* LAN8 */
		{ 1, 1 },

		/* DMZ */
		{ 1 , 3 }
};

inline int cp_get_real_phy_port(int port_num)
{
        if (!cp_is_l30())
        {
                return port_num;
        }

        /* We are L30. In this case:
                0 (LAN1) - 1
                1 (LAN2) - 2
                2 (LAN3) - 3
                3 (LAN4) - 5
                4 (LAN5) - 6
                5 (LAN6) - 7
                6 (DMZ) - 8
                7 (WAN) - 9
        */
        switch (port_num)
        {
                case 0:
                        return 1;
                case 1:
                        return 2;
                case 2:
                        return 3;
                case 3:
                        return 4;
                case 4:
                        return 5;
                case 5:
                        return 6;
                case 6: /*DMZ*/
                        return 8;
                case 7: /*WAN*/
                        return 9;
                default:
                        return port_num;
        }
}

/*******************************************************************************
* mvEthE6065_61PhyBasicInit - 
*
* DESCRIPTION:
*	Do a basic Init to the Phy , including reset
*       
* INPUT:
*       ethPortNum - Ethernet port number
*
* OUTPUT:
*       None.
*
* RETURN:   None
*
*******************************************************************************/
MV_VOID		mvEthE6065_61SwitchBasicInit(MV_U32 ethPortNum)
{
	switchVlanInit(ethPortNum,
			   MV_E6065_CPU_PORT,
			   MV_E6065_MAX_PORTS_NUM,
			   MV_E6065_PORTS_OFFSET,
			   MV_E6065_ENABLED_PORTS);
}

/*******************************************************************************
* mvEthE6063PhyBasicInit - 
*
* DESCRIPTION:
*	Do a basic Init to the Phy , including reset
*       
* INPUT:
*       ethPortNum - Ethernet port number
*
* OUTPUT:
*       None.
*
* RETURN:   None
*
*******************************************************************************/
MV_VOID		mvEthE6063SwitchBasicInit(MV_U32 ethPortNum)
{
	switchVlanInit(ethPortNum,
			   MV_E6063_CPU_PORT,
			   MV_E6063_MAX_PORTS_NUM,
			   MV_E6063_PORTS_OFFSET,
			   MV_E6063_ENABLED_PORTS);
}

/*******************************************************************************
* mvEthE6131PhyBasicInit - 
*
* DESCRIPTION:
*	Do a basic Init to the Phy , including reset
*       
* INPUT:
*       ethPortNum - Ethernet port number
*
* OUTPUT:
*       None.
*
* RETURN:   None
*
*******************************************************************************/
MV_VOID		mvEthE6131SwitchBasicInit(MV_U32 ethPortNum)
{

	MV_U16 reg;

	/*Enable Phy power up*/
	mvEthPhyRegWrite (0,0,0x9140);
	mvEthPhyRegWrite (1,0,0x9140);
	mvEthPhyRegWrite (2,0,0x9140);


	/*Enable PPU*/
	mvEthPhyRegWrite (0x1b,4,0x4080);


	/*Enable Phy detection*/
	mvEthPhyRegRead (0x13,0,&reg);
	reg &= ~(1<<12);
	mvEthPhyRegWrite (0x13,0,reg);

	mvOsDelay(100);
	mvEthPhyRegWrite (0x13,1,0x33);


	switchVlanInit(ethPortNum,
			    MV_E6131_CPU_PORT,
			   MV_E6131_MAX_PORTS_NUM,
			   MV_E6131_PORTS_OFFSET,
			   MV_E6131_ENABLED_PORTS);

}


/*******************************************************************************
* mvEthE6171PhyLedInit -
*
* DESCRIPTION:
*           Change  the LEDn functionalety on all the ports for the required mode
*      
* INPUT:
*       ethPortNum - KW Ethernet port number the switch is connected too.
*           LEDnum - number of switch LED, 0 LED0, 1 LED1, 2 LED2, 3 LED3.
*           mode -  0 link/activity/speed
*                       1 link/activity/speed
*                       2 link/activity/speed
*                       3 link/activity/speed
*                       4 rsrvd
*                       5 rsrvd
*                       6 special
*                       7 duplex collision
*                       8 activity
*                       9 100 link
*                       10 100 link/act
*                       11 10/100 link
*                       12 rsrvd
*                       13 force blink
*                       14 force off
*                       15 force on
*
* OUTPUT:
*       None.
*
* RETURN:   None
*
*******************************************************************************/
MV_VOID                     mvEthE6171SwitchLEDInit_old(MV_U32 ethPortNum, MV_U32 LEDnum, MV_U32 mode)
{
    MV_U32 prt;
    MV_U16 reg;
    MV_U16 devAddr = mvBoardPhyAddrGet(ethPortNum);
 
    /* Pass on all ports */
    for(prt=0; prt < MV_E6171_MAX_PORTS_NUM - 1; prt++)
    {
            if (prt != MV_E6171_1_CPU_PORT  && prt != MV_E6171_2_CPU_PORT)
            {
 
                /* Update LED mode */
                /* Set LED control register for LED 0,1 or 2,3 */
                if ((LEDnum == 0) || (LEDnum == 1))
                        mvEthPhyRegWrite(devAddr, 0, (0x9A00 | (prt << 5) | E6171_LED_CTL_REG));
                else
                {
                        mvEthPhyRegWrite(devAddr, 1, 0x1000);
                        mvEthPhyRegWrite(devAddr, 0, (0x9600 | (prt << 5) | E6171_LED_CTL_REG));
                        mvEthPhyRegWrite(devAddr, 0, (0x9A00 | (prt << 5) | E6171_LED_CTL_REG));
                }
   
                /* Get LED register data */
                mvEthPhyRegRead(devAddr, 1, &reg);
                reg &=0x3ff;
 
                /* Change functionality for LEDn */
                //printf("reg %x\n",reg);
                switch(LEDnum) {
                case 0:
                            reg &= ~0xf;
                            reg |= mode;
                            break;
                case 1:
                            reg &= ~(0xf << 4);
                            reg |= (mode << 4);
                            break;
                case 2:
                            reg &= ~0xf;
                            reg |= mode;
                            break;
                case 3:
                            reg &= ~(0xf << 4);
                            reg |= (mode << 4);
                            break;
                }
                /* Add update bit to LED reg */
                reg |= (1 << 15);
                       
                /* Set LED control register for LED 0,1 or 2,3 */
                if ((LEDnum == 2) || (LEDnum == 3))
                        reg |= (1 << 12);
               
                mvEthPhyRegWrite(devAddr, 1, reg);
                mvEthPhyRegWrite(devAddr, 0, (0x9600 | (prt << 5) | E6171_LED_CTL_REG));
            }
    }
}


/*******************************************************************************
* mvEthE6171PhyLedInit -
*
* DESCRIPTION:
*           Change  the LEDn functionalety on all the ports for the required mode
*      
* INPUT:
*       ethPortNum - KW Ethernet port number the switch is connected too.
*           LEDnum - number of switch LED, 0 LED0, 1 LED1, 2 LED2, 3 LED3.
*           mode -  0 link/activity/speed
*                       1 link/activity/speed
*                       2 link/activity/speed
*                       3 link/activity/speed
*                       4 rsrvd
*                       5 rsrvd
*                       6 special
*                       7 duplex collision
*                       8 activity
*                       9 100 link
*                       10 100 link/act
*                       11 10/100 link
*                       12 rsrvd
*                       13 force blink
*                       14 force off
*                       15 force on
*
* OUTPUT:
*       None.
*
* RETURN:   None
*
*******************************************************************************/
MV_VOID mvEthE6171SwitchLEDInit(MV_U32 LEDnum, MV_U32 mode)
{
	MV_U32 ethPortNum;
	MV_U32 LED_control_reg_num;
    MV_U32 prt;
	MV_U32 _i;
	MV_U32 max_prt;
    MV_U16 reg;

	struct mv_port_info port_info = {-1, -1};
	
    /* Pass on all ports */
	max_prt = cp_is_l30() ? CP_L30_LANS_NUM : CP_L50_LANS_NUM;

	for(_i=0; _i < max_prt; _i++)
	{
		port_info	= mv_port_map[cp_get_real_phy_port(_i)];
		ethPortNum	= port_info.dev_num + 1;
		prt			= port_info.physical_port_num;

		mvEthSwitchRegRead(ethPortNum, 0x10 + prt, E6171_LED_CTL_REG, &reg);
		reg &= 0x3ff; // bits 0-10
		
		/* For LEDs 1 & 3, the data is in bits 4-7 
		 * Set new data on relevant bits */
		if ((LEDnum == 1) || (LEDnum == 3))
		{
			reg &= ~(0xf << 4);
			reg |= (mode << 4);
		}
		else
		{
			reg &= ~0xf;
			reg |= mode;
		}		

		/* Add update bit to LED reg */
		reg |= BIT15;
                       
		/* Set LED control register for LED 0,1 or 2,3 */
		/* For LEDs 2 & 3 the control register is 1 */
		switch (LEDnum)
		{
		case 2:
		case 3:
			LED_control_reg_num = E6171_LEDS_23_CTL;
			break;
		case 0:
		case 1:
		default:
			LED_control_reg_num = E6171_LEDS_01_CTL;
		}
		reg |= LED_control_reg_num << 12;
		
		mvEthSwitchRegWrite(ethPortNum, 0x10 + prt, E6171_LED_CTL_REG, reg);
    }
}

MV_VOID mvEthE6171SwitchLED_Progress_Bar(void)
{
	MV_U32 ethPortNum;
	MV_U32 LED_control_reg_num;
    MV_U32 prt;
	MV_U32 _i;
	MV_U32 max_prt;
    MV_U16 reg, blink_reg, blink_val;
	MV_U32 mode = E6171_LED_FORCE_BLINK;

	struct mv_port_info port_info = {-1, -1};
	
    /* Pass on all ports */
	max_prt = cp_is_l30() ? 6 : 8;

	for(_i=0; _i < max_prt; _i++)
	{
		port_info	= mv_port_map[cp_get_real_phy_port(_i)];
		ethPortNum	= port_info.dev_num + 1;
		prt			= port_info.physical_port_num;

		for (LED_control_reg_num  = E6171_LEDS_01_CTL ; 
			 LED_control_reg_num <= E6171_LEDS_23_CTL ; 
			 LED_control_reg_num++)
		{
			reg = (BIT15 | (LED_control_reg_num << 12) | mode << 4 | mode);
			mvEthSwitchRegWrite(ethPortNum, 0x10 + prt, E6171_LED_CTL_REG, reg);
		}
		
		/* Change blink rate register */

		/* Set LED control register for Strech and blink rate register */
		/* API:
		 * port num is 1 or 2
		 * phyAddr is not really phyAddr. It is internal mapping - 0x10 + port
		 * 6171_LED_CTL_REG - for LED control register
		 * Data:
		 *		Bit 15 for update
		 *		Bits 12-14 - set the LED register we want to write to: 0x06 - blink rate
		 *		Bits 0-10 - for actual data
		 */
		LED_control_reg_num = E6171_LEDS_BLINK_CTL;
		blink_val = 4;
		blink_val <<= 4;
		blink_val += 5 - (_i%2);
		blink_reg = (BIT15 | (LED_control_reg_num << 12) | blink_val);
		
		mvEthSwitchRegWrite(ethPortNum, 0x10 + prt, E6171_LED_CTL_REG, blink_reg);
    }
}


MV_VOID mvEthE6171SwitchLEDInit_reset_blink(void)
{
	MV_U32 ethPortNum;
	MV_U32 LED_control_reg_num;
    MV_U32 prt;
	MV_U32 _i;
	MV_U32 max_prt;
    MV_U16 blink_reg, blink_val;

	struct mv_port_info port_info = {-1, -1};
	
    /* Pass on all ports */
	max_prt = cp_is_l30() ? 6 : 8;

	for(_i=0; _i < max_prt; _i++)
	{
		port_info	= mv_port_map[cp_get_real_phy_port(_i)];
		ethPortNum	= port_info.dev_num + 1;
		prt			= port_info.physical_port_num;

		/* Change blink rate register */

		/* Set LED control register for Strech and blink rate register */
		/* API:
		 * port num is 1 or 2
		 * phyAddr is not really phyAddr. It is internal mapping - 0x10 + port
		 * 6171_LED_CTL_REG - for LED control register
		 * Data:
		 *		Bit 15 for update
		 *		Bits 12-14 - set the LED register we want to write to. In this case - 0x06
		 *		Bits 0-10 - for actual data
		 */
		LED_control_reg_num = E6171_LEDS_BLINK_CTL;
		blink_val = 0x33;
		blink_reg = (BIT15 | (LED_control_reg_num << 12) | blink_val);
		mvEthSwitchRegWrite(ethPortNum, 0x10 + prt, E6171_LED_CTL_REG, blink_reg);
    }
}

/*******************************************************************************
* mvEthE6161PhyLedInit - 
*
* DESCRIPTION:
*	Change  the LEDn functionalety on all the ports for the required mode
*       
* INPUT:
*       ethPortNum - KW Ethernet port number the switch is connected too.
*	LEDnum - number of switch LED, 0 LED0, 1 LED1, 2 LED2.
*	mode -	0 rsrvd,
*		1 on link, blink activity, off no link
*		2 on link, blink receive, off no link
*		3 on activity, off no activity
*		4 blink activity, off no activity
*		5 on transmit, off no transmit
*		6 on 10/1000, off else
*		7 rsrvd
*		8 force off
*		9 force on
*		10 force Hi-Z
*		11 force blink
*		15..12 rsrvd
*
* OUTPUT:
*       None.
*
* RETURN:   None
*
*******************************************************************************/
MV_VOID		mvEthE6161SwitchLEDInit(MV_U32 ethPortNum, MV_U32 LEDnum, MV_U32 mode)
{
    MV_U32 prt;
    MV_U16 reg;
    volatile MV_U32 timeout; 

    /* Pass on all ports */
    for(prt=0; prt < MV_E6161_MAX_PORTS_NUM - 1; prt++)
    {
    	if (prt != MV_E6161_CPU_PORT)
    	{
    	    /* Change page to 3 for LED control access */
#if 0
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 

    				MV_E6161_SMI_PHY_DATA, 0x3);
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
    				MV_E6161_SMI_PHY_COMMAND, (0x9400 | (prt << 5) | 22));
    
    	    /* Make sure SMIBusy bit cleared before another SMI operation can take place*/
    	    timeout = E6161_PHY_TIMEOUT;
    	    do
                {
    	     	mvEthSwitchRegRead(ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_COMMAND,&reg);
    		if(timeout-- == 0)
    		{
    		 	mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
    			return;
    		}
    	    }while (reg & E6161_PHY_SMI_BUSY_MASK);   

#endif
    	    /* Update LED mode */
	    /* Read function control register */
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
    				MV_E6161_SMI_PHY_DATA, 0x0);
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
    				MV_E6161_SMI_PHY_COMMAND, (0x9800 | (prt << 5) | 16));
    
    	    /* Make sure SMIBusy bit cleared before another SMI operation can take place*/
    	    timeout = E6161_PHY_TIMEOUT;
    	    do
                {
    	     	mvEthSwitchRegRead(ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_COMMAND,&reg);
    		if(timeout-- == 0)
    		{
    		 	mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
    			return;
    		}
    	    }while (reg & E6161_PHY_SMI_BUSY_MASK);   

    	    /* Get LED register data */
	    mvEthSwitchRegRead(ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_DATA,&reg);

	    /* Change functionality for LEDn */
	    printf("reg %x\n",reg);
	    switch(LEDnum) {
	    case 0:
		    reg &= ~0xf;
		    reg |= mode;
		    break;
	    case 1:
		    reg &= ~(0xf << 4);
		    reg |= (mode << 4);
		    break;
	    case 2:
		    reg &= ~(0xf << 12);
		    reg |= (mode << 12);
		    break;
	    }

	    /* Write function control register */
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
    				MV_E6161_SMI_PHY_DATA, reg);
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
    				MV_E6161_SMI_PHY_COMMAND, (0x9400 | (prt << 5) | 16));
    
    	    /* Make sure SMIBusy bit cleared before another SMI operation can take place*/
    	    timeout = E6161_PHY_TIMEOUT;
    	    do
                {
    	     	mvEthSwitchRegRead(ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_COMMAND,&reg);
    		if(timeout-- == 0)
    		{
    		 	mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
    			return;
    		}
    	    }while (reg & E6161_PHY_SMI_BUSY_MASK);   

    	    /* Return to page to 0 for control access */
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
    				MV_E6161_SMI_PHY_DATA, 0x0);
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
    				MV_E6161_SMI_PHY_COMMAND, (0x9400 | (prt << 5) | 22));
    
    	    /* Make sure SMIBusy bit cleared before another SMI operation can take place*/
    	    timeout = E6161_PHY_TIMEOUT;
    	    do
                {
    	     	mvEthSwitchRegRead(ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_COMMAND,&reg);
    		if(timeout-- == 0)
    		{
    		 	mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
    			return;
    		}
    	    }while (reg & E6161_PHY_SMI_BUSY_MASK);   
	}
    }
}

/*******************************************************************************
* mvEthE6161PhyBasicInit - 
*
* DESCRIPTION:
*	Do a basic Init to the Phy , including reset
*       
* INPUT:
*       ethPortNum - Ethernet port number
*
* OUTPUT:
*       None.
*
* RETURN:   None
*
*******************************************************************************/
MV_VOID		mvEthE6161SwitchBasicInit(MV_U32 ethPortNum)
{

    MV_U32 prt;
    MV_U16 reg;
    volatile MV_U32 timeout; 

    /* The 6161/5 needs a delay */
    mvOsDelay(1000);

    /* Init vlan */
    switchVlanInit(ethPortNum,
		    MV_E6161_CPU_PORT,
		    MV_E6161_MAX_PORTS_NUM,
		    MV_E6161_PORTS_OFFSET,
		    MV_E6161_ENABLED_PORTS);

    /* Enable RGMII delay on Tx and Rx for CPU port */
    mvEthSwitchRegWrite (ethPortNum, 0x14,0x1a,0x81e7);
    mvEthSwitchRegRead (ethPortNum, 0x15,0x1a,&reg);
    mvEthSwitchRegWrite (ethPortNum, 0x15,0x1a, reg | 0x18);
    mvEthSwitchRegWrite (ethPortNum, 0x14,0x1a,0xc1e7);


    for(prt=0; prt < MV_E6161_MAX_PORTS_NUM - 1; prt++)
    {
    	if (prt != MV_E6161_CPU_PORT)
    	{
    	    /*Enable Phy power up*/
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
    				MV_E6161_SMI_PHY_DATA, 0x3360);
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
    				MV_E6161_SMI_PHY_COMMAND, (0x9410 | (prt << 5)));
    
    	    /*Make sure SMIBusy bit cleared before another SMI operation can take place*/
    	    timeout = E6161_PHY_TIMEOUT;
    	    do
                {
    	     	mvEthSwitchRegRead(ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_COMMAND,&reg);
    		if(timeout-- == 0)
    		{
    		 	mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
    			return;
    		}
    	    }while (reg & E6161_PHY_SMI_BUSY_MASK);   
    
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_DATA,0x1140);
    	    mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_COMMAND,(0x9400 | (prt << 5)));
    
    	    /*Make sure SMIBusy bit cleared before another SMI operation can take place*/
    	    timeout = E6161_PHY_TIMEOUT;
    	    do
                {
    	     	mvEthSwitchRegRead(ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_COMMAND,&reg);
    		if(timeout-- == 0)
    		{
    		 	mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
    			return;
    		}
    	    }while (reg & E6161_PHY_SMI_BUSY_MASK);   
    
    	}
    
    	/*Enable port*/
    	mvEthSwitchRegWrite (ethPortNum, MV_E6161_PORTS_OFFSET + prt, 4, 0x7f);
    
    /*Change MDI port polarity to fix board erratum of reverse connector*/
#define MDI_POLARITY_FIX
#if defined(MDI_POLARITY_FIX) && defined(RD_88F6281A)
        if(mvBoardIdGet() == RD_88F6281A_ID)
        {
    	    /*Make sure SMIBusy bit cleared before another SMI operation can take place*/
    	    timeout = E6161_PHY_TIMEOUT;
    	    do
                {
    	     	mvEthSwitchRegRead(ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR,
    				MV_E6161_SMI_PHY_COMMAND,&reg);
    		if(timeout-- == 0)
    		{
    		 	mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
    			return;
    		}
    	    }while (reg & E6161_PHY_SMI_BUSY_MASK);   
        	mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
                                 MV_E6161_SMI_PHY_DATA, 0xf);
        	mvEthSwitchRegWrite (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
                                 MV_E6161_SMI_PHY_COMMAND, (0x9414 + (prt*0x20)));
        	mvEthSwitchRegRead (ethPortNum, MV_E6161_GLOBAL_2_REG_DEV_ADDR, 
                                MV_E6161_SMI_PHY_DATA, &reg);/* used for delay */
        }
#endif /*MDI_POLARITY_FIX*/
    }

    /*Force CPU port to RGMII FDX 1000Base*/
    mvEthSwitchRegWrite (ethPortNum, MV_E6161_PORTS_OFFSET + MV_E6161_CPU_PORT, 1, 0x3e);

}

/*******************************************************************************
* mvEthE6171PhyBasicInit - 
*
* DESCRIPTION:
*	Do a basic Init to the 2x6171 switch for CUT3 including connection 
*   of port 5 on each switch in RGMII mode
*       
* INPUT:
*       ethPortNum - Ethernet port number
*		shutdown - If not "0", shutdown switch.
*
* OUTPUT:
*       None.
*
* RETURN:   None
*
*******************************************************************************/

MV_VOID		mvEthE6171SwitchBasicInit_ex(MV_U32 ethPortNum, MV_U32 shutdown)
{

    MV_U32 prt,swtch;
    MV_U16 reg;
    volatile MV_U32 timeout; 

    /* The 6171 needs a delay */
    mvOsDelay(1000);

    /* Init vlan of switch 1 and enable all ports */
    switchVlanInit(1,
		    MV_E6171_1_CPU_PORT,
		    MV_E6171_MAX_PORTS_NUM,
		    MV_E6171_PORTS_OFFSET,
		    MV_E6171_ENABLED_PORTS);

    /* Init vlan of switch 2 and enable all ports */
    switchVlanInit(2,
            MV_E6171_2_CPU_PORT,
            MV_E6171_MAX_PORTS_NUM,
            MV_E6171_PORTS_OFFSET,
            MV_E6171_ENABLED_PORTS);

    /* Enable RGMII delay on Tx and Rx for port 5 switch 1 */
    mvEthSwitchRegRead (1, MV_E6171_PORTS_OFFSET + 5, MV_E6171_SWITCH_PHIYSICAL_CTRL_REG, &reg);
    mvEthSwitchRegWrite (1, MV_E6171_PORTS_OFFSET + 5, MV_E6171_SWITCH_PHIYSICAL_CTRL_REG, (reg|0xC000));
    /* Enable RGMII delay on Tx and Rx for port 6 switch 1 */
    mvEthSwitchRegRead (1, MV_E6171_PORTS_OFFSET + 6, MV_E6171_SWITCH_PHIYSICAL_CTRL_REG, &reg);
    mvEthSwitchRegWrite (1, MV_E6171_PORTS_OFFSET + 6, MV_E6171_SWITCH_PHIYSICAL_CTRL_REG, (reg|0xC000));
    /* Enable RGMII delay on Tx and Rx for port 5 switch 2 */
    mvEthSwitchRegRead (2, MV_E6171_PORTS_OFFSET + 6, MV_E6171_SWITCH_PHIYSICAL_CTRL_REG, &reg);
    mvEthSwitchRegWrite (2, MV_E6171_PORTS_OFFSET + 6, MV_E6171_SWITCH_PHIYSICAL_CTRL_REG, (reg|0xC000));
	#ifdef SEATTLE
	 mvEthSwitchRegWrite (2, MV_E6171_PORTS_OFFSET + 6, MV_E6171_SWITCH_PHIYSICAL_CTRL_REG, 0xC0fd);
	#else
    /* Disable port 6 MII switch 2 for ramp up */
    mvEthSwitchRegRead (2, MV_E6171_PORTS_OFFSET + 6, MV_E6171_SWITCH_PHIYSICAL_CTRL_REG, &reg);
    mvEthSwitchRegWrite (2, MV_E6171_PORTS_OFFSET + 6, MV_E6171_SWITCH_PHIYSICAL_CTRL_REG, ((reg|0x10)&~0x20));
	#endif

    /* Power up PHYs */
    for (swtch=1; swtch < 3; swtch++)
    {
        for(prt=0; prt < MV_E6171_MAX_PORTS_NUM - 2; prt++)
        {
        	if (((swtch == 1) && (prt != MV_E6171_1_CPU_PORT)) ||
                ((swtch == 2) && (prt != MV_E6171_2_CPU_PORT)))
        	{
        	    /*Enable Phy power up for switch 1*/
        	    mvEthSwitchRegWrite (swtch, MV_E6171_GLOBAL_2_REG_DEV_ADDR, 
        				MV_E6171_SMI_PHY_DATA, 0x3360);
        	    mvEthSwitchRegWrite (swtch, MV_E6171_GLOBAL_2_REG_DEV_ADDR, 
        				MV_E6171_SMI_PHY_COMMAND, (0x9410 | (prt << 5)));
        
        	    /*Make sure SMIBusy bit cleared before another SMI operation can take place*/
        	    timeout = E6171_PHY_TIMEOUT;
        	    do
                    {
        	     	mvEthSwitchRegRead(swtch, MV_E6171_GLOBAL_2_REG_DEV_ADDR,
        				MV_E6171_SMI_PHY_COMMAND,&reg);
        		if(timeout-- == 0)
        		{
        		 	mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
        			return;
        		}
        	    }while (reg & E6171_PHY_SMI_BUSY_MASK);   

				/* Change  0x1140 to 0x1940 to power down */
        	    mvEthSwitchRegWrite (swtch, MV_E6171_GLOBAL_2_REG_DEV_ADDR,
        				MV_E6171_SMI_PHY_DATA,shutdown ? 0x1940 : 0x1140);
        	    mvEthSwitchRegWrite (swtch, MV_E6171_GLOBAL_2_REG_DEV_ADDR,
        				MV_E6171_SMI_PHY_COMMAND,(0x9400 | (prt << 5)));
        
        	    /*Make sure SMIBusy bit cleared before another SMI operation can take place*/
        	    timeout = E6171_PHY_TIMEOUT;
        	    do
                    {
        	     	mvEthSwitchRegRead(swtch, MV_E6171_GLOBAL_2_REG_DEV_ADDR,
        				MV_E6171_SMI_PHY_COMMAND,&reg);
        		if(timeout-- == 0)
        		{
        		 	mvOsPrintf("mvEthPhyRegRead: SMI busy timeout\n");
        			return;
        		}
        	    }while (reg & E6171_PHY_SMI_BUSY_MASK);   
        
        	}
        }
    }


		  
	mvEthSwitchRegWrite (1, 0x15,
						 0x4, 0x7f);
	
	mvEthSwitchRegWrite (1, E6171_LED_CTL_REG,
						 0x4, 0x7f);
	
	mvEthSwitchRegWrite (2, 0x15,
							0x4, 0x7f);
	
	mvEthSwitchRegWrite (2, E6171_LED_CTL_REG,
							0x4, 0x7f);
}

MV_VOID		mvEthE6171SwitchBasicInit(MV_U32 ethPortNum)
{
	mvEthE6171SwitchBasicInit_ex(ethPortNum, 0);
}

MV_VOID         mvEthE6171SwitchBasicShutdown(MV_U32 ethPortNum)
{
	mvEthE6171SwitchBasicInit_ex(ethPortNum, 1);
}



static void switchVlanInit(MV_U32 ethPortNum,
						   MV_U32 switchCpuPort,
					   MV_U32 switchMaxPortsNum,
					   MV_U32 switchPortsOffset,
					   MV_U32 switchEnabledPortsMask)
{
    MV_U32 prt;
	MV_U16 reg;

	/* be sure all ports are disabled */
    	for(prt=0; prt < switchMaxPortsNum; prt++)
	{
		mvEthSwitchRegRead (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
							  MV_SWITCH_PORT_CONTROL_REG,&reg);
		reg &= ~0x3;
		mvEthSwitchRegWrite (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
							  MV_SWITCH_PORT_CONTROL_REG,reg);

	}

	/* Set CPU port VID to 0x1 */
	mvEthSwitchRegRead (ethPortNum, MV_SWITCH_PORT_OFFSET(switchCpuPort),
						  MV_SWITCH_PORT_VID_REG,&reg);
	reg &= ~0xfff;
	reg |= 0x1;
	mvEthSwitchRegWrite (ethPortNum, MV_SWITCH_PORT_OFFSET(switchCpuPort),
						  MV_SWITCH_PORT_VID_REG,reg);


	/* Setting  Port default priority for all ports to zero */
    	for(prt=0; prt < switchMaxPortsNum; prt++)
	{

		mvEthSwitchRegRead (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
							  MV_SWITCH_PORT_VID_REG,&reg);
		reg &= ~0xc000;
		mvEthSwitchRegWrite (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
							  MV_SWITCH_PORT_VID_REG,reg);
	}

	/* Setting VID and VID map for all ports except CPU port */
    	for(prt=0; prt < switchMaxPortsNum; prt++)
	{

		/* only for enabled ports */
        	if ((1 << prt)& switchEnabledPortsMask)
		{
			/* skip CPU port */
			if (prt== switchCpuPort) continue;

			/* 
			*  set Ports VLAN Mapping.
			*	port prt <--> MV_SWITCH_CPU_PORT VLAN #prt+1.
			*/
	
			mvEthSwitchRegRead (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
								  MV_SWITCH_PORT_VID_REG,&reg);
			reg &= ~0x0fff;
			reg |= (prt+1);
			mvEthSwitchRegWrite (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
								  MV_SWITCH_PORT_VID_REG,reg);
	
	
			/* Set Vlan map table for all ports to send only to MV_SWITCH_CPU_PORT */
			mvEthSwitchRegRead (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
								  MV_SWITCH_PORT_VMAP_REG,&reg);
			reg &= ~((1 << switchMaxPortsNum) - 1);
			reg |= (1 << switchCpuPort);
			mvEthSwitchRegWrite (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
								  MV_SWITCH_PORT_VMAP_REG,reg);
		}

	}


	/* Set Vlan map table for MV_SWITCH_CPU_PORT to see all ports*/
	mvEthSwitchRegRead (ethPortNum, MV_SWITCH_PORT_OFFSET(switchCpuPort),
						  MV_SWITCH_PORT_VMAP_REG,&reg);
	reg &= ~((1 << switchMaxPortsNum) - 1);
	reg |= switchEnabledPortsMask & ~(1 << switchCpuPort);
	mvEthSwitchRegWrite (ethPortNum, MV_SWITCH_PORT_OFFSET(switchCpuPort),
						  MV_SWITCH_PORT_VMAP_REG,reg);


    	/*enable only appropriate ports to forwarding mode - and disable the others*/
    	for(prt=0; prt < switchMaxPortsNum; prt++)
	{

        	if ((1 << prt)& switchEnabledPortsMask)
		{
			mvEthSwitchRegRead (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
								  MV_SWITCH_PORT_CONTROL_REG,&reg);
			reg |= 0x3;
			mvEthSwitchRegWrite (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
								  MV_SWITCH_PORT_CONTROL_REG,reg);

		}
		else
		{
			/* Disable port */
			mvEthSwitchRegRead (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
								  MV_SWITCH_PORT_CONTROL_REG,&reg);
			reg &= ~0x3;
			mvEthSwitchRegWrite (ethPortNum, MV_SWITCH_PORT_OFFSET(prt),
								  MV_SWITCH_PORT_CONTROL_REG,reg);
		}
	}
	return;
}

void mvEthSwitchRegWrite(MV_U32 ethPortNum, MV_U32 phyAddr,
                                 MV_U32 regOffs, MV_U16 data)
{
	MV_U16 reg;
    MV_U32 switchMultiChipMode;
	
    if(mvBoardSmiScanModeGet(ethPortNum) == 0x2)
    {
        switchMultiChipMode = mvBoardPhyAddrGet(ethPortNum);
		do
        {
            mvEthPhyRegRead(switchMultiChipMode, 0x0, &reg);
        } while((reg & BIT15));    // Poll till SMIBusy bit is clear
        mvEthPhyRegWrite(switchMultiChipMode, 0x1, data);   // Write data to Switch indirect data register
        mvEthPhyRegWrite(switchMultiChipMode, 0x0, (MV_U16)(regOffs | (phyAddr << 5) |
                         BIT10 | BIT12 | BIT15));   // Write command to Switch indirect command register
    }
    else
        mvEthPhyRegWrite(phyAddr, regOffs, data);
}

void mvEthSwitchRegRead(MV_U32 ethPortNum, MV_U32 phyAddr,
                             MV_U32 regOffs, MV_U16 *data)
{
	MV_U16 reg;
    MV_U32 switchMultiChipMode;

	if(mvBoardSmiScanModeGet(ethPortNum) == 0x2)
    {
        switchMultiChipMode = mvBoardPhyAddrGet(ethPortNum);
		do
        {
            mvEthPhyRegRead(switchMultiChipMode, 0x0, &reg);
        } while((reg & BIT15));    // Poll till SMIBusy bit is clear
        mvEthPhyRegWrite(switchMultiChipMode, 0x0, (MV_U16)(regOffs | (phyAddr << 5) |
                         BIT11 | BIT12 | BIT15));   // Write command to Switch indirect command register
        do
        {
            mvEthPhyRegRead(switchMultiChipMode, 0, &reg);
        } while((reg & BIT15));    // Poll till SMIBusy bit is clear
        mvEthPhyRegRead(switchMultiChipMode, 0x1, data);   // read data from Switch indirect data register
    }
    else
        mvEthPhyRegRead(phyAddr, regOffs, data);
}
