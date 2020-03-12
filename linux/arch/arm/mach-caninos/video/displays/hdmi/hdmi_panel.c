#include <mach/bootdev.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "../../dss/dss_features.h"

#include "hdmi.h"

static struct {
	/* This protects the panel ops, mainly when accessing the HDMI IP. */
	struct mutex lock;
} hdmi_panel;

static int hdmi_panel_probe(struct owl_dss_device *dssdev)
{
	/* Initialize default timings to VGA in DVI mode */
	
	//owldss_hdmi_display_set_vid(dssdev, &dssdev->timings, 1);
	
	return owldss_hdmi_panel_init(dssdev);
}

static void hdmi_panel_remove(struct owl_dss_device *dssdev)
{
	return;
}

static int hdmi_panel_suspend(struct owl_dss_device *dssdev)
{
	mutex_lock(&hdmi_panel.lock);
	owldss_hdmi_panel_suspend(dssdev);
	mutex_unlock(&hdmi_panel.lock);
	return 0;
}

static int hdmi_panel_resume(struct owl_dss_device *dssdev)
{
	mutex_lock(&hdmi_panel.lock);
	owldss_hdmi_panel_resume(dssdev);
	mutex_unlock(&hdmi_panel.lock);
	return 0;
}

static int hdmi_panel_enable(struct owl_dss_device *dssdev)
{
	int r = 0;
	
	mutex_lock(&hdmi_panel.lock);

	if (dssdev->state == OWL_DSS_DISPLAY_ACTIVE) {
		r = 0;
		goto err;
	}

//	owldss_hdmi_display_set_timing(dssdev, &dssdev->timings);

	r = owldss_hdmi_display_enable(dssdev);
	
	if (r)
	{
		DEBUG_ERR("failed to power on\n");
		goto err;
	}
	
	dssdev->state = OWL_DSS_DISPLAY_ACTIVE;

err:
	mutex_unlock(&hdmi_panel.lock);
	
	return r;
}

static void hdmi_panel_disable(struct owl_dss_device *dssdev)
{
	mutex_lock(&hdmi_panel.lock);

	if (dssdev->state == OWL_DSS_DISPLAY_ACTIVE) {
		owldss_hdmi_display_disable(dssdev);
		dssdev->state = OWL_DSS_DISPLAY_DISABLED;		
	}
	
	mutex_unlock(&hdmi_panel.lock);
}

static void hdmi_get_timings(struct owl_dss_device *dssdev,
			struct owl_video_timings *timings)
{
	mutex_lock(&hdmi_panel.lock);

	*timings = dssdev->timings;

	mutex_unlock(&hdmi_panel.lock);
}

static void hdmi_set_timings(struct owl_dss_device *dssdev,
			struct owl_video_timings *timings)
{
	mutex_lock(&hdmi_panel.lock);
	owldss_hdmi_display_set_timing(dssdev, timings);
	mutex_unlock(&hdmi_panel.lock);
}

static int hdmi_check_timings(struct owl_dss_device *dssdev,
			struct owl_video_timings *timings)
{
	int r = 0;

	mutex_lock(&hdmi_panel.lock);
	r = owldss_hdmi_display_check_timing(dssdev, timings);
	mutex_unlock(&hdmi_panel.lock);
	return r;
}

static void hdmi_set_vid(struct owl_dss_device *dssdev, int vid)
{
	mutex_lock(&hdmi_panel.lock);
	owldss_hdmi_display_set_vid(dssdev, vid);
	mutex_unlock(&hdmi_panel.lock);
}

static void hdmi_get_vid(struct owl_dss_device *dssdev, int *vid)
{
	mutex_lock(&hdmi_panel.lock);
	owldss_hdmi_display_get_vid(dssdev, vid);
	mutex_unlock(&hdmi_panel.lock);
}

static void hdmi_enable_hpd(struct owl_dss_device *dssdev, bool enable)
{
	mutex_lock(&hdmi_panel.lock);
	owldss_hdmi_display_enable_hotplug(dssdev, enable);
	mutex_unlock(&hdmi_panel.lock);
}

static void hdmi_enable_hdcp(struct owl_dss_device *dssdev, bool enable)
{
	mutex_lock(&hdmi_panel.lock);
	owldss_hdmi_display_enable_hdcp(dssdev, enable);
	mutex_unlock(&hdmi_panel.lock);
}

static void hdmi_get_over_scan(struct owl_dss_device *dssdev, u16 * over_scan_width,u16 * over_scan_height)
{
	mutex_lock(&hdmi_panel.lock);
	owldss_hdmi_display_get_overscan(dssdev, over_scan_width,over_scan_height);
	mutex_unlock(&hdmi_panel.lock);
}

static void hdmi_set_over_scan(struct owl_dss_device *dssdev,u16 over_scan_width,u16 over_scan_height)
{
	mutex_lock(&hdmi_panel.lock);
	owldss_hdmi_display_set_overscan(dssdev, over_scan_width,over_scan_height);
	mutex_unlock(&hdmi_panel.lock);
}

static int hdmi_get_vid_cap(struct owl_dss_device *dssdev, int *vid_cap)
{
	int r;
	mutex_lock(&hdmi_panel.lock);
	r = owldss_hdmi_display_get_vid_cap(dssdev, vid_cap);
	mutex_unlock(&hdmi_panel.lock);
	return r;
}

static int hdmi_read_edid(struct owl_dss_device *dssdev,u8 *buf, int len)
{
	int r;
	mutex_lock(&hdmi_panel.lock);
	r = owldss_hdmi_read_edid(dssdev, buf, len);
	mutex_unlock(&hdmi_panel.lock);
	return r;
}

static int hdmi_get_cable_status(struct owl_dss_device *dssdev)
{
	int r;
	mutex_lock(&hdmi_panel.lock);
	r = owldss_hdmi_display_get_cable_status(dssdev);
	mutex_unlock(&hdmi_panel.lock);
	return r;
}

static int generic_hdmi_panel_get_effect_parameter(struct owl_dss_device *dssdev,
                                              enum owl_plane_effect_parameter parameter_id)
{
	return owl_hdmi_get_effect_parameter(dssdev, parameter_id);
}

static void generic_hdmi_panel_set_effect_parameter(struct owl_dss_device *dssdev,
                                           enum owl_plane_effect_parameter parameter_id ,int value)
{
	owl_hdmi_set_effect_parameter(dssdev, parameter_id,value);
}

static struct owl_dss_driver hdmi_driver = {
	.probe			  = hdmi_panel_probe,
	.remove			  = hdmi_panel_remove,
	.suspend		  = hdmi_panel_suspend,
	.resume 		  = hdmi_panel_resume,
	.enable			  = hdmi_panel_enable,
	.disable		  = hdmi_panel_disable,
	.get_timings	  = hdmi_get_timings,
	.set_timings	  = hdmi_set_timings,
	.check_timings	  = hdmi_check_timings,
	.set_vid   	 	  = hdmi_set_vid,
	.get_vid    	  = hdmi_get_vid,
	.enable_hpd		  = hdmi_enable_hpd,
	.enable_hdcp	  = hdmi_enable_hdcp,
	.get_vid_cap 	  = hdmi_get_vid_cap, 
	.get_cable_status = hdmi_get_cable_status,
	.get_effect_parameter   = generic_hdmi_panel_get_effect_parameter,
	.set_effect_parameter   = generic_hdmi_panel_set_effect_parameter,
	.get_over_scan = hdmi_get_over_scan,
	.set_over_scan = hdmi_set_over_scan,
	.read_edid        = hdmi_read_edid,
	
	.driver			= {
		.name   = "hdmi_panel",
		.owner  = THIS_MODULE,
	},
};

int __init hdmi_panel_init(void)
{
   int r = -1; 

	mutex_init(&hdmi_panel.lock);
	
	r = owl_hdmi_init_platform();
	
	if(r) {
		return r;
	}
	
	return owl_dss_register_driver(&hdmi_driver);
}

void __exit hdmi_panel_exit(void)
{
	owl_hdmi_uninit_platform();
	owl_dss_unregister_driver(&hdmi_driver);
}

module_init(hdmi_panel_init);
module_exit(hdmi_panel_exit);

