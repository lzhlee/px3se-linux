
�ð汾֧��AWB,AEC��WDR��Ԥ��blockģʽ����֧��ov2710 tunning file�ļ���

1. ֻ�ṩ��ʱ���豩¶��ͷ�ļ�
HAL/include/
include/shared_ptr.h
include/ebase
include/oslayer

2.��Ҫ��dspģ���ͷ�ļ�

HAL/include/cam_types.h
include/linux/media/rk-isp11-config.h	
include/linux/media/v4l2-config_rockchip.h

  struct HAL_Buffer_MetaData�����¸��£�
    1�� struct HAL_Buffer_MetaData ��Ϣ�������֣�һ����Ϊ���ܱȽϹ��ĵ�ģ�����ã��ⲿ����ת��Ϊ
    ��׼�����ݸ�ʽ����һ����Ϊ��ǰISP�Ĵ�����������ã���Ҫͨ���ض�ת�����������߶���
    ISP datasheet���ܽϺý����
    videoӦ����Ҫ�ڵõ�CameraBuffer�󣬵���
        HAL_Buffer_MetaData* CameraBuffer:: getMetaData()  ��ȡ��Ӧ��metadata��Ȼ��ָ�봫��dsp��
     
struct HAL_Buffer_MetaData {
        /* ��֡��Ӧ��white balance  gain*/
         struct HAL_ISP_awb_gain_cfg_s    wb_gain;
        /* ��֡��Ӧ��filter ��sharp & noise leverl*/
         struct HAL_ISP_flt_cfg_s flt;
        /* ��֡��Ӧ��wdr ������Ϣ����ȷ��dsp����Ҫ֪��ʲô��Ϣ���������ǰ�
        ��ǰwdr�Ĵ�����Ϣ���г�����*/
         struct HAL_ISP_wdr_cfg_s wdr;
        /*��֡��Ӧ��dpf strength��Ϣ*/
         struct HAL_ISP_dpf_strength_cfg_s dpf_strength;
        /*��֡��Ӧ��dpf������Ϣ����ģ��Ĵ����ܶ࣬��ȷ��dsp����Ҫʲô��Ϣ��
         ��δʵ��*/
         struct HAL_ISP_dpf_cfg_s dpf;
        /*��֡��Ӧ���ع�ʱ��������*/
         float exp_time;
         float exp_gain;
        /*��֡��Ӧ��ISP��ģ��ʹ����Ϣ*/
         bool_t enabled[HAL_ISP_MODULE_MAX_ID_ID+1];
        /* ��Ӧ�� include/linux/media/rk-isp11-config.h ��
        struct v4l2_buffer_metadata_s �ṹ��ISP��ģ��
        ��Ӧ����Ϣ���У������ǼĴ�������ģ���Ҫ
        ����ISP datasheet���ܽ��*/
         void* metedata_drv;
};
 
    2�� CamIsp11DevHwItf::configureISPModules(const void* config);
    �ú�������dspֱ�ӵ��ã�dsp��Ҫ��������Ϣ����struct HAL_ISP_cfg_s�ṹ�����ýṹ��ָ��
    ��video��video����CamIsp11DevHwItf::configureISPModules(const void* config)�������á�
    struct HAL_ISP_cfg_s {
        /* ��Ҫ���õ�ģ�鶼Ҫ����maskλ���� HAL_ISP_WDR_MASK*/
         uint32_t updated_mask;
        /*��Ӧģ���ʹ����Ϣ��updated_mask��Ӧλ���ϲ���Ч��
            HAL_ISP_ACTIVE_FALSE�� �رո�ģ��
            HAL_ISP_ACTIVE_SETTING��ʹ���ⲿ���ã�����ģʽʱ����Ҫ�����Ӧ������
                            ��Ϣ��������wdrģ�飬��ô��Ҫ��struct HAL_ISP_wdr_cfg_s *wdr_cfg
                            �ֶν��и�ֵ��
            HAL_ISP_ACTIVE_DEFAULT��Ĭ�����ã���HAL��������Ϊ�����ܴ�tunning file��
                            ȡ�����ã�����ʹ��Ĭ��д�������ã�������3Aģ����ơ�ÿ��ģ��
                            Ĭ�϶�Ϊdefault���á�
        */
         enum HAL_ISP_ACTIVE_MODE enabled[HAL_ISP_MODULE_MAX_ID_ID+1];
    };
