#pragma once

#include <boost\noncopyable.hpp>

#include "common.h"

namespace common
{
	class Meas
	{
	public:
		/*typedef std::shared_ptr<Meas> PMeas;*/
	public:
		Meas();
		~Meas();
	public:
		Id   id;                     // �������������
		Time time;                   // ����� ���������
		Flag source;                 // ������� ����� ��� ����������� ��������
		Flag flag;                   // ����� �� ������ �������
		Value data;                  // ���� ������
	};
}