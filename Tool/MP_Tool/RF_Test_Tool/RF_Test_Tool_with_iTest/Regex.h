// Regex.h: interface for the CRegex class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REGEX_H__1AA17863_7891_46A5_BD05_BA387C1D1A14__INCLUDED_)
#define AFX_REGEX_H__1AA17863_7891_46A5_BD05_BA387C1D1A14__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#pragma warning(disable: 4786)
#include "IRegex.h"
class CRegex  
{
public:
	CRegex();
	virtual ~CRegex();
	//////////////////////////////////////////////////////////////////////////
    //����˵��:
    //�ַ�������,��pszSource=abc123cde pszExpre=(\d*).* ����Ϊ[0]123cde [1]123
    //����˵��:
    //pszSource[IN]:Դ�ַ���
    //pszExpre[IN]:������ʽ
    //vecResult[OUT]:���ҵõ��Ľ��
    //strError[OUT]:ʧ��ʱ���صĴ�����Ϣ
    //����˵��:
    //���ҳɹ�����true,����ʧ�ܷ���false
    //////////////////////////////////////////////////////////////////////////
    virtual bool fnSearch(const char *pszSource,const char* pszExpre,std::vector<std::string>& vecResult,std::string& strError);
    //////////////////////////////////////////////////////////////////////////
    //����˵��:
    //�ַ���ƥ��,��pszSource=abc123cde321 pszExpre=\d* ����Ϊ[0]123  [1]321
    //����˵��:
    //pszSource[IN]:Դ�ַ���
    //pszExpre[IN]:������ʽ
    //vecResult[OUT]:ƥ��õ��Ľ��
    //strError[OUT]:ʧ��ʱ���صĴ�����Ϣ
    //����˵��:
    //ƥ��ɹ�����true,����ʧ�ܷ���false
    //////////////////////////////////////////////////////////////////////////
    virtual bool fnGrep(const char *pszSource,const char* pszExpre,std::vector<std::string>& vecResult,std::string& strError);
    //////////////////////////////////////////////////////////////////////////
    //����˵��:
    //����ʽ�滻,�ҳ�������ʽһ�µ��ַ������滻
    //����˵��:
    //pszSource[IN]:Դ�ַ���
    //pszExpre[IN]:������ʽ
    //pszNewString[IN]:���滻�����ַ���
    //strResult[OUT]:�滻��Ľ��
    //strError[OUT]:ʧ��ʱ���صĴ�����Ϣ
    //����˵��:
    //ƥ��ɹ�����true,����ʧ�ܷ���false
    //////////////////////////////////////////////////////////////////////////
    virtual bool fnRegReplace(const char *pszSource,const char* pszExpre,const char* pszNewString,std::string &strResult,std::string& strError);
	
private:


};

#endif // !defined(AFX_REGEX_H__1AA17863_7891_46A5_BD05_BA387C1D1A14__INCLUDED_)
